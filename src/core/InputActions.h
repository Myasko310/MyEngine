#pragma once

#include <array>
#include <string>
#include <unordered_map>
#include <vector>

namespace MyEngine
{
	class InputActions
	{
	public:
		enum class InputContext
		{
			Gameplay = 0,
			UI = 1,
			Console = 2
		};
		struct ActionBinding
		{
			std::vector<int> keys;           // GLFW_KEY_*
			std::vector<int> mouseButtons;   // GLFW_MOUSE_BUTTON_*
			std::vector<int> gamepadButtons; // GLFW_GAMEPAD_BUTTON_*
			std::vector<int> gamepadAxes;    // optional analog-to-action
			float axisThreshold = 0.5f;      // action active when abs(axis) >= threshold
			bool invertAxis = false;
			InputContext context = InputContext::Gameplay;
		};

		struct AxisBinding
		{
			std::vector<std::pair<int, int>> keyPairs;       // {positiveKey, negativeKey}
			std::vector<int>                 gamepadAxes;    // GLFW_GAMEPAD_AXIS_*
			std::vector<std::pair<int, int>> gamepadButtonPairs; // {positiveBtn, negativeBtn}
			bool invert = false;
			float deadzone = -1.0f;  // <= 0 uses global deadzone
			float sensitivity = 1.0f;
			InputContext context = InputContext::Gameplay;
		};

		struct AxisCalibration
		{
			float deadzone = -1.0f;       // <= 0 uses global gamepad deadzone
			float exponent = 1.0f;        // response curve exponent
			float saturation = 1.0f;      // scale before clamp
			bool invert = false;
		};

		struct Profile
		{
			std::unordered_map<std::string, ActionBinding> actions;
			std::unordered_map<std::string, AxisBinding> axes;
			std::array<AxisCalibration, 6> axisCalibration{};
		};

		static void Update();
		static void RegisterDefaults();

		struct ContextLayer
		{
			InputContext context = InputContext::Gameplay;
			int priority = 0;
			bool blocksLowerPriority = false;
		};

		// Profiles
		static std::vector<std::string> GetProfileNames();
		static std::string GetActiveProfileName();
		static std::string GetDefaultProfileName();
		static bool SetActiveProfile(const std::string& name);
		static bool SetDefaultProfile(const std::string& name);
		static bool CreateProfile(const std::string& name, bool copyCurrent = true);
		static bool DeleteProfile(const std::string& name);
		static bool RenameProfile(const std::string& oldName, const std::string& newName);
		static bool DuplicateProfile(const std::string& sourceName, const std::string& newName);
		static bool IsBindingsDirty();

		// Context layering
		static void PushContextLayer(InputContext context, int priority, bool blocksLowerPriority);
		static void PopContextLayer(InputContext context);
		static void ClearContextLayers();
		static std::vector<ContextLayer> GetContextLayers();
		static bool IsContextAllowed(InputContext context);

		// Actions
		static bool IsAction(const std::string& name);
		static bool IsAction(const std::string& name, InputContext context);
		static bool IsActionPressed(const std::string& name);
		static bool IsActionReleased(const std::string& name);

		// Axes
		static float GetAxis(const std::string& name);
		static float GetAxis(const std::string& name, InputContext context);

		// Bindings
		static void BindAction(const std::string& name, const ActionBinding& binding);
		static void BindAxis(const std::string& name, const AxisBinding& binding);
		static void ClearAction(const std::string& name);
		static void ClearAxis(const std::string& name);
		static bool HasAction(const std::string& name);
		static bool HasAxis(const std::string& name);
		static std::vector<std::string> GetActionNames();
		static std::vector<std::string> GetAxisNames();
		static bool TryGetActionBinding(const std::string& name, ActionBinding& outBinding);
		static bool TryGetAxisBinding(const std::string& name, AxisBinding& outBinding);

		// Conflict resolution
		static void ResolveConflictsKeepFirst();
		static void ResolveConflictsKeepLast();

		// Gamepad
		static bool  IsGamepadConnected();
		static bool  IsGamepadButtonPressed(int button);
		static float GetGamepadAxisRaw(int axis);
		static const unsigned char* GetGamepadButtonsRaw();
		static const float* GetGamepadAxesRaw();
		static int GetGamepadButtonCount();
		static int GetGamepadAxisCount();
		static float GetGamepadAxisCalibrated(int axis);
		static bool  TryGetGamepadAxisCalibration(int axis, AxisCalibration& outCalibration);
		static bool  SetGamepadAxisCalibration(int axis, const AxisCalibration& calibration);
		static void  ResetGamepadCalibration();
		static void  SetGamepadDeadzone(float deadzone);
		static float GetGamepadDeadzone();

		// Persistence
		static bool SaveBindings(const std::string& path);
		static bool LoadBindings(const std::string& path);

	private:
		static void SyncActiveProfileFromCurrent();
		static void LoadCurrentFromActiveProfile();
		static float ApplyAxisCalibration(int axis, float rawValue);
		static void MarkBindingsDirty();
		static bool EvaluateAction(const ActionBinding& binding);
		static float EvaluateAxis(const AxisBinding& binding);
		static bool IsBindingContextAllowed(InputContext bindingContext);

		static std::unordered_map<std::string, ActionBinding> s_actions;
		static std::unordered_map<std::string, AxisBinding>   s_axes;
		static std::unordered_map<std::string, Profile> s_profiles;
		static std::string s_activeProfile;
		static std::string s_defaultProfile;

		static std::unordered_map<std::string, bool> s_currentActionState;
		static std::unordered_map<std::string, bool> s_previousActionState;
		static std::vector<ContextLayer> s_contextLayers;

		static float s_gamepadDeadzone;
		static bool  s_bindingsDirty;
		static int   s_gamepadId;
		static bool  s_gamepadStateValid;
		static unsigned char s_gamepadButtons[15];
		static unsigned char s_prevGamepadButtons[15];
		static float s_gamepadAxes[6];
	};
}
