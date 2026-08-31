#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace MyEngine
{
	class InputActions
	{
	public:
		struct ActionBinding
		{
			std::vector<int> keys;           // GLFW_KEY_*
			std::vector<int> mouseButtons;   // GLFW_MOUSE_BUTTON_*
			std::vector<int> gamepadButtons; // GLFW_GAMEPAD_BUTTON_*
			std::vector<int> gamepadAxes;    // optional analog-to-action
			float axisThreshold = 0.5f;      // action active when abs(axis) >= threshold
			bool invertAxis = false;
		};

		struct AxisBinding
		{
			std::vector<std::pair<int, int>> keyPairs;       // {positiveKey, negativeKey}
			std::vector<int>                 gamepadAxes;    // GLFW_GAMEPAD_AXIS_*
			std::vector<std::pair<int, int>> gamepadButtonPairs; // {positiveBtn, negativeBtn}
			bool invert = false;
			float deadzone = -1.0f;  // <= 0 uses global deadzone
			float sensitivity = 1.0f;
		};

		struct Profile
		{
			std::unordered_map<std::string, ActionBinding> actions;
			std::unordered_map<std::string, AxisBinding> axes;
		};

		static void Update();
		static void RegisterDefaults();

		// Profiles
		static std::vector<std::string> GetProfileNames();
		static std::string GetActiveProfileName();
		static bool SetActiveProfile(const std::string& name);
		static bool CreateProfile(const std::string& name, bool copyCurrent = true);
		static bool DeleteProfile(const std::string& name);

		// Actions
		static bool IsAction(const std::string& name);
		static bool IsActionPressed(const std::string& name);
		static bool IsActionReleased(const std::string& name);

		// Axes
		static float GetAxis(const std::string& name);

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
		static void  SetGamepadDeadzone(float deadzone);
		static float GetGamepadDeadzone();

		// Persistence
		static bool SaveBindings(const std::string& path);
		static bool LoadBindings(const std::string& path);

	private:
		static void SyncActiveProfileFromCurrent();
		static void LoadCurrentFromActiveProfile();
		static bool EvaluateAction(const ActionBinding& binding);
		static float EvaluateAxis(const AxisBinding& binding);

		static std::unordered_map<std::string, ActionBinding> s_actions;
		static std::unordered_map<std::string, AxisBinding>   s_axes;
		static std::unordered_map<std::string, Profile> s_profiles;
		static std::string s_activeProfile;

		static std::unordered_map<std::string, bool> s_currentActionState;
		static std::unordered_map<std::string, bool> s_previousActionState;

		static float s_gamepadDeadzone;
		static int   s_gamepadId;
		static bool  s_gamepadStateValid;
		static unsigned char s_gamepadButtons[15];
		static unsigned char s_prevGamepadButtons[15];
		static float s_gamepadAxes[6];
	};
}
