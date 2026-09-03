#include "core/InputActions.h"
#include "core/Input.h"

#include <GLFW/glfw3.h>

#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <unordered_set>

namespace MyEngine
{
	std::unordered_map<std::string, InputActions::ActionBinding> InputActions::s_actions;
	std::unordered_map<std::string, InputActions::AxisBinding>   InputActions::s_axes;
	std::unordered_map<std::string, InputActions::Profile> InputActions::s_profiles;
	std::string InputActions::s_activeProfile = "Default";
	std::string InputActions::s_defaultProfile = "Default";
	std::unordered_map<std::string, bool> InputActions::s_currentActionState;
	std::unordered_map<std::string, bool> InputActions::s_previousActionState;
	std::vector<InputActions::ContextLayer> InputActions::s_contextLayers;

	float InputActions::s_gamepadDeadzone = 0.15f;
	bool  InputActions::s_bindingsDirty = false;
	int   InputActions::s_gamepadId = -1;
	bool  InputActions::s_gamepadStateValid = false;
	unsigned char InputActions::s_gamepadButtons[15] = {};
	unsigned char InputActions::s_prevGamepadButtons[15] = {};
	float InputActions::s_gamepadAxes[6] = {};

	void InputActions::SyncActiveProfileFromCurrent()
	{
		auto& profile = s_profiles[s_activeProfile];
		profile.actions = s_actions;
		profile.axes = s_axes;
	}

	void InputActions::MarkBindingsDirty()
	{
		s_bindingsDirty = true;
	}

	void InputActions::LoadCurrentFromActiveProfile()
	{
		auto it = s_profiles.find(s_activeProfile);
		if (it == s_profiles.end())
		{
			s_actions.clear();
			s_axes.clear();
			s_profiles[s_activeProfile] = Profile{};
			return;
		}

		s_actions = it->second.actions;
		s_axes = it->second.axes;
		s_currentActionState.clear();
		s_previousActionState.clear();
		for (const auto& [name, _] : s_actions)
		{
			s_currentActionState[name] = false;
			s_previousActionState[name] = false;
		}
	}

	float InputActions::ApplyAxisCalibration(int axis, float rawValue)
	{
		auto it = s_profiles.find(s_activeProfile);
		if (it == s_profiles.end() || axis < 0 || axis >= static_cast<int>(it->second.axisCalibration.size()))
			return rawValue;

		const AxisCalibration& calibration = it->second.axisCalibration[axis];
		float deadzone = calibration.deadzone > 0.0f ? std::clamp(calibration.deadzone, 0.0f, 0.95f) : s_gamepadDeadzone;
		float magnitude = std::fabs(rawValue);
		if (magnitude <= deadzone)
			return 0.0f;

		float normalized = (magnitude - deadzone) / (1.0f - deadzone);
		normalized = std::clamp(normalized, 0.0f, 1.0f);
		normalized = std::pow(normalized, std::max(0.1f, calibration.exponent));
		normalized *= std::max(0.01f, calibration.saturation);
		normalized = std::clamp(normalized, 0.0f, 1.0f);

		float signedValue = (rawValue >= 0.0f ? normalized : -normalized);
		if (calibration.invert)
			signedValue = -signedValue;
		return signedValue;
	}

	void InputActions::Update()
	{
		// --- Gamepad polling ---
		std::memcpy(s_prevGamepadButtons, s_gamepadButtons, sizeof(s_gamepadButtons));
		s_gamepadStateValid = false;

		// Keep using the current gamepad if still present, otherwise scan
		if (s_gamepadId >= 0 &&
			!(glfwJoystickPresent(s_gamepadId) && glfwJoystickIsGamepad(s_gamepadId)))
		{
			s_gamepadId = -1;
		}
		if (s_gamepadId < 0)
		{
			for (int jid = GLFW_JOYSTICK_1; jid <= GLFW_JOYSTICK_LAST; ++jid)
			{
				if (glfwJoystickPresent(jid) && glfwJoystickIsGamepad(jid))
				{
					s_gamepadId = jid;
					break;
				}
			}
		}

		if (Input::IsInputPlayback())
		{
			if (Input::TryGetPlaybackGamepadState(s_gamepadButtons, static_cast<int>(sizeof(s_gamepadButtons)), s_gamepadAxes, static_cast<int>(sizeof(s_gamepadAxes) / sizeof(float))))
				s_gamepadStateValid = true;
		}
		else
		{
			if (s_gamepadId >= 0)
			{
				GLFWgamepadstate state;
				if (glfwGetGamepadState(s_gamepadId, &state))
				{
					std::memcpy(s_gamepadButtons, state.buttons, sizeof(s_gamepadButtons));
					std::memcpy(s_gamepadAxes, state.axes, sizeof(s_gamepadAxes));
					s_gamepadStateValid = true;
				}
			}
		}
		if (!s_gamepadStateValid)
		{
			std::memset(s_gamepadButtons, 0, sizeof(s_gamepadButtons));
			std::memset(s_gamepadAxes, 0, sizeof(s_gamepadAxes));
		}

		// --- Latch action states for edge detection ---
		s_previousActionState = s_currentActionState;
		for (const auto& [name, binding] : s_actions)
			s_currentActionState[name] = EvaluateAction(binding);
	}

	void InputActions::RegisterDefaults()
	{
		s_actions.clear();
		s_axes.clear();
		if (s_activeProfile.empty())
			s_activeProfile = "Default";
		if (s_defaultProfile.empty())
			s_defaultProfile = "Default";

		ActionBinding jump;
		jump.keys = { GLFW_KEY_SPACE };
		jump.gamepadButtons = { GLFW_GAMEPAD_BUTTON_A };
		jump.context = InputContext::Gameplay;
		BindAction("Jump", jump);

		ActionBinding fire;
		fire.mouseButtons = { GLFW_MOUSE_BUTTON_LEFT };
		fire.gamepadButtons = { GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER };
		fire.context = InputContext::Gameplay;
		BindAction("Fire", fire);

		ActionBinding interact;
		interact.keys = { GLFW_KEY_E };
		interact.gamepadButtons = { GLFW_GAMEPAD_BUTTON_X };
		interact.context = InputContext::Gameplay;
		BindAction("Interact", interact);

		ActionBinding sprint;
		sprint.keys = { GLFW_KEY_LEFT_SHIFT };
		sprint.gamepadButtons = { GLFW_GAMEPAD_BUTTON_LEFT_THUMB };
		sprint.context = InputContext::Gameplay;
		BindAction("Sprint", sprint);

		ActionBinding crouch;
		crouch.keys = { GLFW_KEY_LEFT_CONTROL };
		crouch.gamepadButtons = { GLFW_GAMEPAD_BUTTON_B };
		crouch.context = InputContext::Gameplay;
		BindAction("Crouch", crouch);

		ActionBinding fight;
		fight.keys = { GLFW_KEY_F };
		fight.mouseButtons = { GLFW_MOUSE_BUTTON_RIGHT };
		fight.gamepadButtons = { GLFW_GAMEPAD_BUTTON_Y };
		fight.context = InputContext::Gameplay;
		BindAction("Fight", fight);

		ActionBinding pause;
		pause.keys = { GLFW_KEY_ESCAPE };
		pause.gamepadButtons = { GLFW_GAMEPAD_BUTTON_START };
		pause.context = InputContext::UI;
		BindAction("Pause", pause);

		AxisBinding moveForward;
		moveForward.keyPairs = { { GLFW_KEY_W, GLFW_KEY_S } };
		moveForward.gamepadAxes = { GLFW_GAMEPAD_AXIS_LEFT_Y };
		moveForward.invert = true;
		moveForward.context = InputContext::Gameplay;
		BindAxis("MoveForward", moveForward);

		AxisBinding moveRight;
		moveRight.keyPairs = { { GLFW_KEY_D, GLFW_KEY_A } };
		moveRight.gamepadAxes = { GLFW_GAMEPAD_AXIS_LEFT_X };
		moveRight.context = InputContext::Gameplay;
		BindAxis("MoveRight", moveRight);

		AxisBinding lookX;
		lookX.gamepadAxes = { GLFW_GAMEPAD_AXIS_RIGHT_X };
		lookX.context = InputContext::Gameplay;
		BindAxis("LookX", lookX);

		AxisBinding lookY;
		lookY.gamepadAxes = { GLFW_GAMEPAD_AXIS_RIGHT_Y };
		lookY.invert = true;
		lookY.context = InputContext::Gameplay;
		BindAxis("LookY", lookY);

		SyncActiveProfileFromCurrent();
		ClearContextLayers();
		PushContextLayer(InputContext::Gameplay, 0, false);
		s_bindingsDirty = false;
	}

	std::vector<std::string> InputActions::GetProfileNames()
	{
		std::vector<std::string> names;
		names.reserve(s_profiles.size());
		for (const auto& [name, _] : s_profiles)
			names.push_back(name);
		if (names.empty())
			names.push_back(s_activeProfile);
		std::sort(names.begin(), names.end());
		return names;
	}

	std::string InputActions::GetActiveProfileName()
	{
		return s_activeProfile;
	}

	std::string InputActions::GetDefaultProfileName()
	{
		return s_defaultProfile;
	}

	bool InputActions::SetActiveProfile(const std::string& name)
	{
		if (name.empty() || s_profiles.find(name) == s_profiles.end())
			return false;
		SyncActiveProfileFromCurrent();
		s_activeProfile = name;
		LoadCurrentFromActiveProfile();
		return true;
	}

	bool InputActions::SetDefaultProfile(const std::string& name)
	{
		if (name.empty() || s_profiles.find(name) == s_profiles.end())
			return false;
		s_defaultProfile = name;
		MarkBindingsDirty();
		return true;
	}

	bool InputActions::CreateProfile(const std::string& name, bool copyCurrent)
	{
		if (name.empty() || s_profiles.find(name) != s_profiles.end())
			return false;
		Profile p;
		if (copyCurrent)
		{
			p.actions = s_actions;
			p.axes = s_axes;
			auto activeIt = s_profiles.find(s_activeProfile);
			if (activeIt != s_profiles.end())
				p.axisCalibration = activeIt->second.axisCalibration;
		}
		s_profiles[name] = std::move(p);
		MarkBindingsDirty();
		return true;
	}

	bool InputActions::DeleteProfile(const std::string& name)
	{
		if (name.empty() || s_profiles.size() <= 1 || s_profiles.find(name) == s_profiles.end())
			return false;
		const bool deletingActive = (name == s_activeProfile);
		const bool deletingDefault = (name == s_defaultProfile);
		s_profiles.erase(name);
		if (deletingActive)
		{
			s_activeProfile = s_defaultProfile;
			if (s_profiles.find(s_activeProfile) == s_profiles.end())
				s_activeProfile = s_profiles.begin()->first;
			LoadCurrentFromActiveProfile();
		}
		if (deletingDefault)
			s_defaultProfile = s_activeProfile;
		MarkBindingsDirty();
		return true;
	}

	bool InputActions::RenameProfile(const std::string& oldName, const std::string& newName)
	{
		if (oldName.empty() || newName.empty() || oldName == newName)
			return false;
		auto it = s_profiles.find(oldName);
		if (it == s_profiles.end() || s_profiles.find(newName) != s_profiles.end())
			return false;

		Profile moved = std::move(it->second);
		s_profiles.erase(it);
		s_profiles[newName] = std::move(moved);
		if (s_activeProfile == oldName)
			s_activeProfile = newName;
		if (s_defaultProfile == oldName)
			s_defaultProfile = newName;
		MarkBindingsDirty();
		return true;
	}

	bool InputActions::DuplicateProfile(const std::string& sourceName, const std::string& newName)
	{
		if (sourceName.empty() || newName.empty() || sourceName == newName)
			return false;
		auto it = s_profiles.find(sourceName);
		if (it == s_profiles.end() || s_profiles.find(newName) != s_profiles.end())
			return false;
		s_profiles[newName] = it->second;
		MarkBindingsDirty();
		return true;
	}

	bool InputActions::IsBindingsDirty()
	{
		return s_bindingsDirty;
	}

	bool InputActions::IsAction(const std::string& name)
	{
		auto it = s_currentActionState.find(name);
		if (it == s_currentActionState.end() || !it->second)
			return false;
		ActionBinding binding;
		if (TryGetActionBinding(name, binding))
			return IsBindingContextAllowed(binding.context);
		return true;
	}

	bool InputActions::IsAction(const std::string& name, InputContext context)
	{
		if (!IsContextAllowed(context))
			return false;
		auto it = s_currentActionState.find(name);
		if (it == s_currentActionState.end() || !it->second)
			return false;
		ActionBinding binding;
		if (TryGetActionBinding(name, binding))
			return binding.context == context;
		return false;
	}

	bool InputActions::IsActionPressed(const std::string& name)
	{
		auto cur = s_currentActionState.find(name);
		if (cur == s_currentActionState.end() || !cur->second)
			return false;
		auto prev = s_previousActionState.find(name);
		return prev == s_previousActionState.end() || !prev->second;
	}

	bool InputActions::IsActionReleased(const std::string& name)
	{
		auto cur = s_currentActionState.find(name);
		bool curDown = cur != s_currentActionState.end() && cur->second;
		if (curDown)
			return false;
		auto prev = s_previousActionState.find(name);
		return prev != s_previousActionState.end() && prev->second;
	}

	float InputActions::GetAxis(const std::string& name)
	{
		auto it = s_axes.find(name);
		if (it == s_axes.end())
			return 0.0f;
		if (!IsBindingContextAllowed(it->second.context))
			return 0.0f;
		return EvaluateAxis(it->second);
	}

	float InputActions::GetAxis(const std::string& name, InputContext context)
	{
		auto it = s_axes.find(name);
		if (it == s_axes.end())
			return 0.0f;
		if (it->second.context != context || !IsContextAllowed(context))
			return 0.0f;
		return EvaluateAxis(it->second);
	}

	void InputActions::BindAction(const std::string& name, const ActionBinding& binding)
	{
		s_actions[name] = binding;
		s_currentActionState.emplace(name, false);
		SyncActiveProfileFromCurrent();
		MarkBindingsDirty();
	}

	void InputActions::PushContextLayer(InputContext context, int priority, bool blocksLowerPriority)
	{
		auto existing = std::find_if(s_contextLayers.begin(), s_contextLayers.end(),
			[&](const ContextLayer& layer) { return layer.context == context; });
		if (existing != s_contextLayers.end())
		{
			existing->priority = priority;
			existing->blocksLowerPriority = blocksLowerPriority;
		}
		else
		{
			s_contextLayers.push_back({ context, priority, blocksLowerPriority });
		}
		std::sort(s_contextLayers.begin(), s_contextLayers.end(),
			[](const ContextLayer& a, const ContextLayer& b) { return a.priority > b.priority; });
	}

	void InputActions::PopContextLayer(InputContext context)
	{
		s_contextLayers.erase(std::remove_if(s_contextLayers.begin(), s_contextLayers.end(),
			[&](const ContextLayer& layer) { return layer.context == context; }), s_contextLayers.end());
	}

	void InputActions::ClearContextLayers()
	{
		s_contextLayers.clear();
	}

	std::vector<InputActions::ContextLayer> InputActions::GetContextLayers()
	{
		return s_contextLayers;
	}

	bool InputActions::IsContextAllowed(InputContext context)
	{
		if (s_contextLayers.empty())
			return context == InputContext::Gameplay;

		const ContextLayer* topLayer = &s_contextLayers.front();
		if (topLayer->context == context)
			return true;
		if (!topLayer->blocksLowerPriority)
			return true;

		for (const auto& layer : s_contextLayers)
		{
			if (layer.context == context)
				return layer.priority >= topLayer->priority;
		}
		return false;
	}

	void InputActions::BindAxis(const std::string& name, const AxisBinding& binding)
	{
		s_axes[name] = binding;
		SyncActiveProfileFromCurrent();
		MarkBindingsDirty();
	}

	void InputActions::ClearAction(const std::string& name)
	{
		s_actions.erase(name);
		s_currentActionState.erase(name);
		s_previousActionState.erase(name);
		SyncActiveProfileFromCurrent();
		MarkBindingsDirty();
	}

	void InputActions::ClearAxis(const std::string& name)
	{
		s_axes.erase(name);
		SyncActiveProfileFromCurrent();
		MarkBindingsDirty();
	}

	bool InputActions::HasAction(const std::string& name)
	{
		return s_actions.count(name) != 0;
	}

	bool InputActions::HasAxis(const std::string& name)
	{
		return s_axes.count(name) != 0;
	}

	std::vector<std::string> InputActions::GetActionNames()
	{
		std::vector<std::string> names;
		names.reserve(s_actions.size());
		for (const auto& [name, _] : s_actions)
			names.push_back(name);
		std::sort(names.begin(), names.end());
		return names;
	}

	std::vector<std::string> InputActions::GetAxisNames()
	{
		std::vector<std::string> names;
		names.reserve(s_axes.size());
		for (const auto& [name, _] : s_axes)
			names.push_back(name);
		std::sort(names.begin(), names.end());
		return names;
	}

	bool InputActions::TryGetActionBinding(const std::string& name, ActionBinding& outBinding)
	{
		auto it = s_actions.find(name);
		if (it == s_actions.end())
			return false;
		outBinding = it->second;
		return true;
	}

	bool InputActions::TryGetAxisBinding(const std::string& name, AxisBinding& outBinding)
	{
		auto it = s_axes.find(name);
		if (it == s_axes.end())
			return false;
		outBinding = it->second;
		return true;
	}

	void InputActions::ResolveConflictsKeepFirst()
	{
		std::unordered_set<int> usedActionKeys, usedActionMouse, usedActionPad;
		for (const auto& actionName : GetActionNames())
		{
			ActionBinding b;
			if (!TryGetActionBinding(actionName, b))
				continue;
			ActionBinding out = b;
			if (!out.keys.empty() && usedActionKeys.count(out.keys[0])) out.keys.clear();
			else if (!out.keys.empty()) usedActionKeys.insert(out.keys[0]);
			if (!out.mouseButtons.empty() && usedActionMouse.count(out.mouseButtons[0])) out.mouseButtons.clear();
			else if (!out.mouseButtons.empty()) usedActionMouse.insert(out.mouseButtons[0]);
			if (!out.gamepadButtons.empty() && usedActionPad.count(out.gamepadButtons[0])) out.gamepadButtons.clear();
			else if (!out.gamepadButtons.empty()) usedActionPad.insert(out.gamepadButtons[0]);
			BindAction(actionName, out);
		}

		std::unordered_set<int> usedAxisKeys, usedAxisPad;
		for (const auto& axisName : GetAxisNames())
		{
			AxisBinding b;
			if (!TryGetAxisBinding(axisName, b))
				continue;
			AxisBinding out = b;
			if (!out.keyPairs.empty())
			{
				int pos = out.keyPairs[0].first;
				int neg = out.keyPairs[0].second;
				if (pos >= 0 && usedAxisKeys.count(pos)) pos = -1; else if (pos >= 0) usedAxisKeys.insert(pos);
				if (neg >= 0 && usedAxisKeys.count(neg)) neg = -1; else if (neg >= 0) usedAxisKeys.insert(neg);
				out.keyPairs[0] = { pos, neg };
			}
			if (!out.gamepadAxes.empty() && usedAxisPad.count(out.gamepadAxes[0])) out.gamepadAxes.clear();
			else if (!out.gamepadAxes.empty()) usedAxisPad.insert(out.gamepadAxes[0]);
			BindAxis(axisName, out);
		}
	}

	void InputActions::ResolveConflictsKeepLast()
	{
		auto actionNames = GetActionNames();
		std::reverse(actionNames.begin(), actionNames.end());
		std::unordered_set<int> usedActionKeys, usedActionMouse, usedActionPad;
		for (const auto& actionName : actionNames)
		{
			ActionBinding b;
			if (!TryGetActionBinding(actionName, b))
				continue;
			ActionBinding out = b;
			if (!out.keys.empty() && usedActionKeys.count(out.keys[0])) out.keys.clear();
			else if (!out.keys.empty()) usedActionKeys.insert(out.keys[0]);
			if (!out.mouseButtons.empty() && usedActionMouse.count(out.mouseButtons[0])) out.mouseButtons.clear();
			else if (!out.mouseButtons.empty()) usedActionMouse.insert(out.mouseButtons[0]);
			if (!out.gamepadButtons.empty() && usedActionPad.count(out.gamepadButtons[0])) out.gamepadButtons.clear();
			else if (!out.gamepadButtons.empty()) usedActionPad.insert(out.gamepadButtons[0]);
			BindAction(actionName, out);
		}

		auto axisNames = GetAxisNames();
		std::reverse(axisNames.begin(), axisNames.end());
		std::unordered_set<int> usedAxisKeys, usedAxisPad;
		for (const auto& axisName : axisNames)
		{
			AxisBinding b;
			if (!TryGetAxisBinding(axisName, b))
				continue;
			AxisBinding out = b;
			if (!out.keyPairs.empty())
			{
				int pos = out.keyPairs[0].first;
				int neg = out.keyPairs[0].second;
				if (pos >= 0 && usedAxisKeys.count(pos)) pos = -1; else if (pos >= 0) usedAxisKeys.insert(pos);
				if (neg >= 0 && usedAxisKeys.count(neg)) neg = -1; else if (neg >= 0) usedAxisKeys.insert(neg);
				out.keyPairs[0] = { pos, neg };
			}
			if (!out.gamepadAxes.empty() && usedAxisPad.count(out.gamepadAxes[0])) out.gamepadAxes.clear();
			else if (!out.gamepadAxes.empty()) usedAxisPad.insert(out.gamepadAxes[0]);
			BindAxis(axisName, out);
		}
	}

	bool InputActions::IsGamepadConnected()
	{
		return s_gamepadStateValid;
	}

	bool InputActions::IsGamepadButtonPressed(int button)
	{
		if (!s_gamepadStateValid || button < 0 || button >= 15)
			return false;
		return s_gamepadButtons[button] == GLFW_PRESS && s_prevGamepadButtons[button] != GLFW_PRESS;
	}

	float InputActions::GetGamepadAxisRaw(int axis)
	{
		if (!s_gamepadStateValid || axis < 0 || axis >= 6)
			return 0.0f;
		return s_gamepadAxes[axis];
	}

	const unsigned char* InputActions::GetGamepadButtonsRaw()
	{
		return s_gamepadButtons;
	}

	const float* InputActions::GetGamepadAxesRaw()
	{
		return s_gamepadAxes;
	}

	int InputActions::GetGamepadButtonCount()
	{
		return static_cast<int>(sizeof(s_gamepadButtons));
	}

	int InputActions::GetGamepadAxisCount()
	{
		return static_cast<int>(sizeof(s_gamepadAxes) / sizeof(float));
	}

	float InputActions::GetGamepadAxisCalibrated(int axis)
	{
		if (!s_gamepadStateValid || axis < 0 || axis >= 6)
			return 0.0f;
		return ApplyAxisCalibration(axis, s_gamepadAxes[axis]);
	}

	bool InputActions::TryGetGamepadAxisCalibration(int axis, AxisCalibration& outCalibration)
	{
		auto it = s_profiles.find(s_activeProfile);
		if (it == s_profiles.end() || axis < 0 || axis >= static_cast<int>(it->second.axisCalibration.size()))
			return false;
		outCalibration = it->second.axisCalibration[axis];
		return true;
	}

	bool InputActions::SetGamepadAxisCalibration(int axis, const AxisCalibration& calibration)
	{
		auto it = s_profiles.find(s_activeProfile);
		if (it == s_profiles.end() || axis < 0 || axis >= static_cast<int>(it->second.axisCalibration.size()))
			return false;
		AxisCalibration clamped = calibration;
		clamped.deadzone = std::clamp(clamped.deadzone, -1.0f, 0.95f);
		clamped.exponent = std::clamp(clamped.exponent, 0.1f, 4.0f);
		clamped.saturation = std::clamp(clamped.saturation, 0.01f, 2.0f);
		it->second.axisCalibration[axis] = clamped;
		MarkBindingsDirty();
		return true;
	}

	void InputActions::ResetGamepadCalibration()
	{
		auto it = s_profiles.find(s_activeProfile);
		if (it == s_profiles.end())
			return;
		for (auto& calibration : it->second.axisCalibration)
			calibration = AxisCalibration{};
		MarkBindingsDirty();
	}

	void InputActions::SetGamepadDeadzone(float deadzone)
	{
		s_gamepadDeadzone = std::clamp(deadzone, 0.0f, 0.95f);
		MarkBindingsDirty();
	}

	float InputActions::GetGamepadDeadzone()
	{
		return s_gamepadDeadzone;
	}

	bool InputActions::EvaluateAction(const ActionBinding& binding)
	{
		if (!IsBindingContextAllowed(binding.context))
			return false;
		for (int key : binding.keys)
			if (Input::IsKeyDown(key))
				return true;
		for (int button : binding.mouseButtons)
			if (Input::IsMouseButtonDown(button))
				return true;
		if (s_gamepadStateValid)
		{
			for (int button : binding.gamepadButtons)
			{
				if (button >= 0 && button < 15 && s_gamepadButtons[button] == GLFW_PRESS)
					return true;
			}
			float threshold = std::clamp(binding.axisThreshold, 0.01f, 1.0f);
			for (int axis : binding.gamepadAxes)
			{
				if (axis < 0 || axis >= 6)
					continue;
				float value = ApplyAxisCalibration(axis, s_gamepadAxes[axis]);
				if (binding.invertAxis)
					value = -value;
				if (std::fabs(value) >= threshold)
					return true;
			}
		}
		return false;
	}

	float InputActions::EvaluateAxis(const AxisBinding& binding)
	{
		if (!IsBindingContextAllowed(binding.context))
			return 0.0f;
		float value = 0.0f;

		for (const auto& [positiveKey, negativeKey] : binding.keyPairs)
		{
			if (Input::IsKeyDown(positiveKey)) value += 1.0f;
			if (Input::IsKeyDown(negativeKey)) value -= 1.0f;
		}

		if (s_gamepadStateValid)
		{
			for (int axis : binding.gamepadAxes)
			{
				if (axis < 0 || axis >= 6)
					continue;
				float calibrated = ApplyAxisCalibration(axis, s_gamepadAxes[axis]);
				if (binding.deadzone > 0.0f)
				{
					float localDeadzone = std::clamp(binding.deadzone, 0.0f, 0.95f);
					if (std::fabs(calibrated) <= localDeadzone)
						calibrated = 0.0f;
					else
					{
						float sign = calibrated > 0.0f ? 1.0f : -1.0f;
						float scaled = (std::fabs(calibrated) - localDeadzone) / (1.0f - localDeadzone);
						calibrated = sign * std::clamp(scaled, 0.0f, 1.0f);
					}
				}
				value += calibrated;
			}
			for (const auto& [positiveBtn, negativeBtn] : binding.gamepadButtonPairs)
			{
				if (positiveBtn >= 0 && positiveBtn < 15 && s_gamepadButtons[positiveBtn] == GLFW_PRESS)
					value += 1.0f;
				if (negativeBtn >= 0 && negativeBtn < 15 && s_gamepadButtons[negativeBtn] == GLFW_PRESS)
					value -= 1.0f;
			}
		}

		value *= std::max(binding.sensitivity, 0.01f);
		value = std::clamp(value, -1.0f, 1.0f);
		return binding.invert ? -value : value;
	}

	bool InputActions::IsBindingContextAllowed(InputContext bindingContext)
	{
		return IsContextAllowed(bindingContext);
	}

	// ---------------- Persistence ----------------

	bool InputActions::SaveBindings(const std::string& path)
	{
		SyncActiveProfileFromCurrent();

		rapidjson::Document doc;
		doc.SetObject();
		auto& alloc = doc.GetAllocator();
		doc.AddMember("gamepadDeadzone", s_gamepadDeadzone, alloc);
		doc.AddMember("activeProfile", rapidjson::Value(s_activeProfile.c_str(), alloc), alloc);

		auto writeActionMap = [&](const std::unordered_map<std::string, ActionBinding>& actionMap, rapidjson::Value& out)
		{
			out.SetObject();
			for (const auto& [name, binding] : actionMap)
			{
				rapidjson::Value obj(rapidjson::kObjectType);
				rapidjson::Value keys(rapidjson::kArrayType);
				for (int k : binding.keys) keys.PushBack(k, alloc);
				obj.AddMember("keys", keys, alloc);
				rapidjson::Value mouse(rapidjson::kArrayType);
				for (int b : binding.mouseButtons) mouse.PushBack(b, alloc);
				obj.AddMember("mouseButtons", mouse, alloc);
				rapidjson::Value pad(rapidjson::kArrayType);
				for (int b : binding.gamepadButtons) pad.PushBack(b, alloc);
				obj.AddMember("gamepadButtons", pad, alloc);
				rapidjson::Value padAxes(rapidjson::kArrayType);
				for (int a : binding.gamepadAxes) padAxes.PushBack(a, alloc);
				obj.AddMember("gamepadAxes", padAxes, alloc);
				obj.AddMember("axisThreshold", binding.axisThreshold, alloc);
				obj.AddMember("invertAxis", binding.invertAxis, alloc);
				obj.AddMember("context", static_cast<int>(binding.context), alloc);
				out.AddMember(rapidjson::Value(name.c_str(), alloc), obj, alloc);
			}
		};

		auto writeAxisMap = [&](const std::unordered_map<std::string, AxisBinding>& axisMap, rapidjson::Value& out)
		{
			out.SetObject();
			for (const auto& [name, binding] : axisMap)
			{
				rapidjson::Value obj(rapidjson::kObjectType);
				rapidjson::Value keyPairs(rapidjson::kArrayType);
				for (const auto& [pos, neg] : binding.keyPairs)
				{
					rapidjson::Value pair(rapidjson::kArrayType);
					pair.PushBack(pos, alloc);
					pair.PushBack(neg, alloc);
					keyPairs.PushBack(pair, alloc);
				}
				obj.AddMember("keyPairs", keyPairs, alloc);
				rapidjson::Value padAxes(rapidjson::kArrayType);
				for (int a : binding.gamepadAxes) padAxes.PushBack(a, alloc);
				obj.AddMember("gamepadAxes", padAxes, alloc);
				rapidjson::Value padPairs(rapidjson::kArrayType);
				for (const auto& [pos, neg] : binding.gamepadButtonPairs)
				{
					rapidjson::Value pair(rapidjson::kArrayType);
					pair.PushBack(pos, alloc);
					pair.PushBack(neg, alloc);
					padPairs.PushBack(pair, alloc);
				}
				obj.AddMember("gamepadButtonPairs", padPairs, alloc);
				obj.AddMember("invert", binding.invert, alloc);
				obj.AddMember("deadzone", binding.deadzone, alloc);
				obj.AddMember("sensitivity", binding.sensitivity, alloc);
				obj.AddMember("context", static_cast<int>(binding.context), alloc);
				out.AddMember(rapidjson::Value(name.c_str(), alloc), obj, alloc);
			}
		};

		rapidjson::Value profiles(rapidjson::kObjectType);
		for (const auto& [profileName, profile] : s_profiles)
		{
			rapidjson::Value profileObj(rapidjson::kObjectType);
			rapidjson::Value actions(rapidjson::kObjectType);
			rapidjson::Value axes(rapidjson::kObjectType);
			rapidjson::Value calibrationArray(rapidjson::kArrayType);
			writeActionMap(profile.actions, actions);
			writeAxisMap(profile.axes, axes);
			for (const auto& calibration : profile.axisCalibration)
			{
				rapidjson::Value calibrationObj(rapidjson::kObjectType);
				calibrationObj.AddMember("deadzone", calibration.deadzone, alloc);
				calibrationObj.AddMember("exponent", calibration.exponent, alloc);
				calibrationObj.AddMember("saturation", calibration.saturation, alloc);
				calibrationObj.AddMember("invert", calibration.invert, alloc);
				calibrationArray.PushBack(calibrationObj, alloc);
			}
			profileObj.AddMember("actions", actions, alloc);
			profileObj.AddMember("axes", axes, alloc);
			profileObj.AddMember("axisCalibration", calibrationArray, alloc);
			profiles.AddMember(rapidjson::Value(profileName.c_str(), alloc), profileObj, alloc);
		}
		doc.AddMember("profiles", profiles, alloc);
		doc.AddMember("defaultProfile", rapidjson::Value(s_defaultProfile.c_str(), alloc), alloc);

		// Legacy compatibility: duplicate active profile into root actions/axes.
		rapidjson::Value rootActions(rapidjson::kObjectType);
		rapidjson::Value rootAxes(rapidjson::kObjectType);
		writeActionMap(s_actions, rootActions);
		writeAxisMap(s_axes, rootAxes);
		doc.AddMember("actions", rootActions, alloc);
		doc.AddMember("axes", rootAxes, alloc);

		rapidjson::StringBuffer buffer;
		rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
		doc.Accept(writer);
		std::ofstream file(path);
		if (!file.is_open())
		{
			std::cerr << "[InputActions] Failed to open " << path << " for writing" << std::endl;
			return false;
		}
		file << buffer.GetString();
		s_bindingsDirty = false;
		return true;
	}

	bool InputActions::LoadBindings(const std::string& path)
	{
		std::ifstream file(path);
		if (!file.is_open())
			return false;
		std::stringstream ss;
		ss << file.rdbuf();
		std::string content = ss.str();

		rapidjson::Document doc;
		doc.Parse(content.c_str());
		if (doc.HasParseError() || !doc.IsObject())
		{
			std::cerr << "[InputActions] Failed to parse " << path << std::endl;
			return false;
		}

		if (doc.HasMember("gamepadDeadzone") && doc["gamepadDeadzone"].IsNumber())
			SetGamepadDeadzone(doc["gamepadDeadzone"].GetFloat());

		auto readActionMap = [](const rapidjson::Value& actionsObj, std::unordered_map<std::string, ActionBinding>& out)
		{
			out.clear();
			if (!actionsObj.IsObject()) return;
			for (auto it = actionsObj.MemberBegin(); it != actionsObj.MemberEnd(); ++it)
			{
				ActionBinding binding;
				const auto& obj = it->value;
				if (!obj.IsObject()) continue;
				if (obj.HasMember("keys") && obj["keys"].IsArray())
					for (const auto& v : obj["keys"].GetArray()) if (v.IsInt()) binding.keys.push_back(v.GetInt());
				if (obj.HasMember("mouseButtons") && obj["mouseButtons"].IsArray())
					for (const auto& v : obj["mouseButtons"].GetArray()) if (v.IsInt()) binding.mouseButtons.push_back(v.GetInt());
				if (obj.HasMember("gamepadButtons") && obj["gamepadButtons"].IsArray())
					for (const auto& v : obj["gamepadButtons"].GetArray()) if (v.IsInt()) binding.gamepadButtons.push_back(v.GetInt());
				if (obj.HasMember("gamepadAxes") && obj["gamepadAxes"].IsArray())
					for (const auto& v : obj["gamepadAxes"].GetArray()) if (v.IsInt()) binding.gamepadAxes.push_back(v.GetInt());
				if (obj.HasMember("axisThreshold") && obj["axisThreshold"].IsNumber()) binding.axisThreshold = obj["axisThreshold"].GetFloat();
				if (obj.HasMember("invertAxis") && obj["invertAxis"].IsBool()) binding.invertAxis = obj["invertAxis"].GetBool();
				if (obj.HasMember("context") && obj["context"].IsInt())
					binding.context = static_cast<InputContext>(std::clamp(obj["context"].GetInt(), 0, 2));
				out[it->name.GetString()] = binding;
			}
		};

		auto readAxisMap = [](const rapidjson::Value& axesObj, std::unordered_map<std::string, AxisBinding>& out)
		{
			out.clear();
			if (!axesObj.IsObject()) return;
			for (auto it = axesObj.MemberBegin(); it != axesObj.MemberEnd(); ++it)
			{
				AxisBinding binding;
				const auto& obj = it->value;
				if (!obj.IsObject()) continue;
				if (obj.HasMember("keyPairs") && obj["keyPairs"].IsArray())
					for (const auto& pair : obj["keyPairs"].GetArray())
						if (pair.IsArray() && pair.Size() == 2 && pair[0].IsInt() && pair[1].IsInt())
							binding.keyPairs.emplace_back(pair[0].GetInt(), pair[1].GetInt());
				if (obj.HasMember("gamepadAxes") && obj["gamepadAxes"].IsArray())
					for (const auto& v : obj["gamepadAxes"].GetArray()) if (v.IsInt()) binding.gamepadAxes.push_back(v.GetInt());
				if (obj.HasMember("gamepadButtonPairs") && obj["gamepadButtonPairs"].IsArray())
					for (const auto& pair : obj["gamepadButtonPairs"].GetArray())
						if (pair.IsArray() && pair.Size() == 2 && pair[0].IsInt() && pair[1].IsInt())
							binding.gamepadButtonPairs.emplace_back(pair[0].GetInt(), pair[1].GetInt());
				if (obj.HasMember("invert") && obj["invert"].IsBool()) binding.invert = obj["invert"].GetBool();
				if (obj.HasMember("deadzone") && obj["deadzone"].IsNumber()) binding.deadzone = obj["deadzone"].GetFloat();
				if (obj.HasMember("sensitivity") && obj["sensitivity"].IsNumber()) binding.sensitivity = obj["sensitivity"].GetFloat();
				if (obj.HasMember("context") && obj["context"].IsInt())
					binding.context = static_cast<InputContext>(std::clamp(obj["context"].GetInt(), 0, 2));
				out[it->name.GetString()] = binding;
			}
		};

		s_profiles.clear();
		if (doc.HasMember("profiles") && doc["profiles"].IsObject())
		{
			for (auto it = doc["profiles"].MemberBegin(); it != doc["profiles"].MemberEnd(); ++it)
			{
				if (!it->value.IsObject()) continue;
				Profile profile;
				if (it->value.HasMember("actions")) readActionMap(it->value["actions"], profile.actions);
				if (it->value.HasMember("axes")) readAxisMap(it->value["axes"], profile.axes);
				if (it->value.HasMember("axisCalibration") && it->value["axisCalibration"].IsArray())
				{
					const auto& calibrationArray = it->value["axisCalibration"].GetArray();
					for (rapidjson::SizeType axis = 0; axis < calibrationArray.Size() && axis < profile.axisCalibration.size(); ++axis)
					{
						if (!calibrationArray[axis].IsObject())
							continue;
						auto calibration = profile.axisCalibration[axis];
						const auto& obj = calibrationArray[axis];
						if (obj.HasMember("deadzone") && obj["deadzone"].IsNumber()) calibration.deadzone = obj["deadzone"].GetFloat();
						if (obj.HasMember("exponent") && obj["exponent"].IsNumber()) calibration.exponent = obj["exponent"].GetFloat();
						if (obj.HasMember("saturation") && obj["saturation"].IsNumber()) calibration.saturation = obj["saturation"].GetFloat();
						if (obj.HasMember("invert") && obj["invert"].IsBool()) calibration.invert = obj["invert"].GetBool();
						profile.axisCalibration[axis] = calibration;
					}
				}
				s_profiles[it->name.GetString()] = std::move(profile);
			}
		}

		if (s_profiles.empty())
		{
			Profile legacy;
			if (doc.HasMember("actions")) readActionMap(doc["actions"], legacy.actions);
			if (doc.HasMember("axes")) readAxisMap(doc["axes"], legacy.axes);
			s_profiles["Default"] = std::move(legacy);
		}

		s_activeProfile = "Default";
		if (doc.HasMember("activeProfile") && doc["activeProfile"].IsString())
			s_activeProfile = doc["activeProfile"].GetString();
		if (s_profiles.find(s_activeProfile) == s_profiles.end())
			s_activeProfile = s_profiles.begin()->first;

		s_defaultProfile = s_activeProfile;
		if (doc.HasMember("defaultProfile") && doc["defaultProfile"].IsString())
			s_defaultProfile = doc["defaultProfile"].GetString();
		if (s_profiles.find(s_defaultProfile) == s_profiles.end())
			s_defaultProfile = s_activeProfile;

		LoadCurrentFromActiveProfile();
		s_bindingsDirty = false;
		return true;
	}
}
