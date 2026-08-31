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
	std::unordered_map<std::string, bool> InputActions::s_currentActionState;
	std::unordered_map<std::string, bool> InputActions::s_previousActionState;

	float InputActions::s_gamepadDeadzone = 0.15f;
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

		ActionBinding jump;
		jump.keys = { GLFW_KEY_SPACE };
		jump.gamepadButtons = { GLFW_GAMEPAD_BUTTON_A };
		BindAction("Jump", jump);

		ActionBinding fire;
		fire.mouseButtons = { GLFW_MOUSE_BUTTON_LEFT };
		fire.gamepadButtons = { GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER };
		BindAction("Fire", fire);

		ActionBinding interact;
		interact.keys = { GLFW_KEY_E };
		interact.gamepadButtons = { GLFW_GAMEPAD_BUTTON_X };
		BindAction("Interact", interact);

		ActionBinding sprint;
		sprint.keys = { GLFW_KEY_LEFT_SHIFT };
		sprint.gamepadButtons = { GLFW_GAMEPAD_BUTTON_LEFT_THUMB };
		BindAction("Sprint", sprint);

		ActionBinding crouch;
		crouch.keys = { GLFW_KEY_LEFT_CONTROL };
		crouch.gamepadButtons = { GLFW_GAMEPAD_BUTTON_B };
		BindAction("Crouch", crouch);

		ActionBinding pause;
		pause.keys = { GLFW_KEY_ESCAPE };
		pause.gamepadButtons = { GLFW_GAMEPAD_BUTTON_START };
		BindAction("Pause", pause);

		AxisBinding moveForward;
		moveForward.keyPairs = { { GLFW_KEY_W, GLFW_KEY_S } };
		moveForward.gamepadAxes = { GLFW_GAMEPAD_AXIS_LEFT_Y };
		moveForward.invert = true;
		BindAxis("MoveForward", moveForward);

		AxisBinding moveRight;
		moveRight.keyPairs = { { GLFW_KEY_D, GLFW_KEY_A } };
		moveRight.gamepadAxes = { GLFW_GAMEPAD_AXIS_LEFT_X };
		BindAxis("MoveRight", moveRight);

		AxisBinding lookX;
		lookX.gamepadAxes = { GLFW_GAMEPAD_AXIS_RIGHT_X };
		BindAxis("LookX", lookX);

		AxisBinding lookY;
		lookY.gamepadAxes = { GLFW_GAMEPAD_AXIS_RIGHT_Y };
		lookY.invert = true;
		BindAxis("LookY", lookY);

		SyncActiveProfileFromCurrent();
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

	bool InputActions::SetActiveProfile(const std::string& name)
	{
		if (name.empty() || s_profiles.find(name) == s_profiles.end())
			return false;
		SyncActiveProfileFromCurrent();
		s_activeProfile = name;
		LoadCurrentFromActiveProfile();
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
		}
		s_profiles[name] = std::move(p);
		return true;
	}

	bool InputActions::DeleteProfile(const std::string& name)
	{
		if (name.empty() || s_profiles.size() <= 1 || s_profiles.find(name) == s_profiles.end())
			return false;
		const bool deletingActive = (name == s_activeProfile);
		s_profiles.erase(name);
		if (deletingActive)
		{
			s_activeProfile = s_profiles.begin()->first;
			LoadCurrentFromActiveProfile();
		}
		return true;
	}

	bool InputActions::IsAction(const std::string& name)
	{
		auto it = s_currentActionState.find(name);
		return it != s_currentActionState.end() && it->second;
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
		return EvaluateAxis(it->second);
	}

	void InputActions::BindAction(const std::string& name, const ActionBinding& binding)
	{
		s_actions[name] = binding;
		s_currentActionState.emplace(name, false);
		SyncActiveProfileFromCurrent();
	}

	void InputActions::BindAxis(const std::string& name, const AxisBinding& binding)
	{
		s_axes[name] = binding;
		SyncActiveProfileFromCurrent();
	}

	void InputActions::ClearAction(const std::string& name)
	{
		s_actions.erase(name);
		s_currentActionState.erase(name);
		s_previousActionState.erase(name);
		SyncActiveProfileFromCurrent();
	}

	void InputActions::ClearAxis(const std::string& name)
	{
		s_axes.erase(name);
		SyncActiveProfileFromCurrent();
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

	void InputActions::SetGamepadDeadzone(float deadzone)
	{
		s_gamepadDeadzone = std::clamp(deadzone, 0.0f, 0.95f);
	}

	float InputActions::GetGamepadDeadzone()
	{
		return s_gamepadDeadzone;
	}

	bool InputActions::EvaluateAction(const ActionBinding& binding)
	{
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
				float value = s_gamepadAxes[axis];
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
		float value = 0.0f;

		for (const auto& [positiveKey, negativeKey] : binding.keyPairs)
		{
			if (Input::IsKeyDown(positiveKey)) value += 1.0f;
			if (Input::IsKeyDown(negativeKey)) value -= 1.0f;
		}

		if (s_gamepadStateValid)
		{
			float deadzone = (binding.deadzone > 0.0f) ? std::clamp(binding.deadzone, 0.0f, 0.95f) : s_gamepadDeadzone;
			for (int axis : binding.gamepadAxes)
			{
				if (axis < 0 || axis >= 6)
					continue;
				float raw = s_gamepadAxes[axis];
				if (std::fabs(raw) > deadzone)
				{
					float sign = raw > 0.0f ? 1.0f : -1.0f;
					float scaled = (std::fabs(raw) - deadzone) / (1.0f - deadzone);
					value += sign * scaled;
				}
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
				out.AddMember(rapidjson::Value(name.c_str(), alloc), obj, alloc);
			}
		};

		rapidjson::Value profiles(rapidjson::kObjectType);
		for (const auto& [profileName, profile] : s_profiles)
		{
			rapidjson::Value profileObj(rapidjson::kObjectType);
			rapidjson::Value actions(rapidjson::kObjectType);
			rapidjson::Value axes(rapidjson::kObjectType);
			writeActionMap(profile.actions, actions);
			writeAxisMap(profile.axes, axes);
			profileObj.AddMember("actions", actions, alloc);
			profileObj.AddMember("axes", axes, alloc);
			profiles.AddMember(rapidjson::Value(profileName.c_str(), alloc), profileObj, alloc);
		}
		doc.AddMember("profiles", profiles, alloc);

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

		LoadCurrentFromActiveProfile();
		return true;
	}
}
