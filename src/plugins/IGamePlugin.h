#pragma once

#include <string>
#include <memory>
#include <cstdint>

class Scene;

namespace MyEngine::Plugins
{
	// ============================================================
	// IGamePlugin Interface
	// ============================================================
	// Abstract base class for game plugins. Plugins implement this
	// interface and are dynamically loaded at runtime.
	//
	// Lifecycle:
	// 1. Plugin library is loaded via DLL/.so
	// 2. CreatePlugin() factory function is called
	// 3. OnInitialize() is called once at startup
	// 4. OnUpdate() is called every frame
	// 5. OnDestroy() is called before unloading
	// ============================================================
	class IGamePlugin
	{
	public:
		virtual ~IGamePlugin() = default;

		// Unique identifier for this plugin
		virtual std::string GetPluginName() const = 0;
		virtual std::string GetPluginVersion() const = 0;

		// Called once when plugin is loaded
		virtual bool OnInitialize(Scene* scene) = 0;

		// Called every frame
		virtual void OnUpdate(Scene* scene, float deltaTime) = 0;

		// Called before plugin is unloaded
		virtual void OnDestroy(Scene* scene) = 0;

		// Optional: called when plugin should render debug UI
		virtual void OnDebugUI() {}

		// Query plugin capabilities
		virtual bool HasFeature(const std::string& featureName) const
		{
			return false; // Override if plugin supports specific features
		}
	};

	// ============================================================
	// Plugin Factory Function Signature
	// ============================================================
	// Plugins must export a function matching this signature
	// to be dynamically loaded by the PluginManager
	// ============================================================
	using CreatePluginFunc = IGamePlugin * (*)();
	using DestroyPluginFunc = void (*)(IGamePlugin*);

	// These symbols must be exported from plugin DLLs
	extern "C"
	{
		// Create plugin instance
		IGamePlugin* CreatePlugin();
		// Destroy plugin instance
		void DestroyPlugin(IGamePlugin* plugin);
	}
}
