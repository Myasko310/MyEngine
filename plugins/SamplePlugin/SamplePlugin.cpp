#include "plugins/PluginSDK.h"

namespace
{
	const MyEngine::Plugins::HostApi* g_HostApi = nullptr;

	void Log(MyEngine::Plugins::PluginLogLevel level, const char* message)
	{
		if (g_HostApi && g_HostApi->Log)
			g_HostApi->Log(level, message);
	}

	bool OnLoad(const MyEngine::Plugins::HostApi* hostApi)
	{
		g_HostApi = hostApi;
		if (g_HostApi && g_HostApi->RegisterNetReplicationHooks)
		{
			if (!g_HostApi->RegisterNetReplicationHooks("SamplePlugin"))
			{
				Log(MyEngine::Plugins::PluginLogLevel::Error, "SamplePlugin failed to register net replication hooks.");
				return false;
			}
		}
		Log(MyEngine::Plugins::PluginLogLevel::Info, "SamplePlugin loaded.");
		return true;
	}

	void OnUnload()
	{
		Log(MyEngine::Plugins::PluginLogLevel::Info, "SamplePlugin unloaded.");
		g_HostApi = nullptr;
	}
}

extern "C" __declspec(dllexport) bool MyEnginePlugin_GetApi(
	std::uint32_t engineSdkMajor,
	std::uint32_t engineSdkMinor,
	MyEngine::Plugins::PluginApi* outApi)
{
	if (!outApi)
		return false;

	if (engineSdkMajor != MyEngine::Plugins::kPluginSdkVersionMajor)
		return false;

	if (engineSdkMinor < MyEngine::Plugins::kPluginSdkVersionMinor)
		return false;

	outApi->sdkVersionMajor = MyEngine::Plugins::kPluginSdkVersionMajor;
	outApi->sdkVersionMinor = MyEngine::Plugins::kPluginSdkVersionMinor;
	outApi->pluginVersionMajor = 1u;
	outApi->pluginVersionMinor = 0u;
	outApi->pluginVersionPatch = 0u;
	outApi->declaredCapabilities =
		static_cast<std::uint64_t>(MyEngine::Plugins::PluginCapability_LifecycleHooks) |
		static_cast<std::uint64_t>(MyEngine::Plugins::PluginCapability_HostLogging) |
		static_cast<std::uint64_t>(MyEngine::Plugins::PluginCapability_NetReplicationHooks);
	outApi->pluginName = "SamplePlugin";
	outApi->OnLoad = &OnLoad;
	outApi->OnUnload = &OnUnload;
	return true;
}
