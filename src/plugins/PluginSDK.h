#pragma once

#include <cstdint>

namespace MyEngine::Plugins
{
	static constexpr std::uint32_t kPluginSdkVersionMajor = 1;
	static constexpr std::uint32_t kPluginSdkVersionMinor = 0;

	enum class PluginLogLevel : std::uint32_t
	{
		Info = 0,
		Warning = 1,
		Error = 2
	};

	enum PluginCapabilityFlags : std::uint64_t
	{
		PluginCapability_None = 0,
		PluginCapability_LifecycleHooks = 1ull << 0,
		PluginCapability_HostLogging = 1ull << 1,
		PluginCapability_NetReplicationHooks = 1ull << 2
	};

	using RegisterNetReplicationHooksFn = bool(*)(const char* pluginName);

	struct HostApi
	{
		std::uint32_t sdkVersionMajor = kPluginSdkVersionMajor;
		std::uint32_t sdkVersionMinor = kPluginSdkVersionMinor;
		void (*Log)(PluginLogLevel level, const char* message) = nullptr;
		RegisterNetReplicationHooksFn RegisterNetReplicationHooks = nullptr;
	};

	struct PluginApi
	{
		std::uint32_t sdkVersionMajor = 0;
		std::uint32_t sdkVersionMinor = 0;
		std::uint32_t pluginVersionMajor = 0;
		std::uint32_t pluginVersionMinor = 0;
		std::uint32_t pluginVersionPatch = 0;
		std::uint64_t declaredCapabilities = PluginCapability_None;
		const char* pluginName = nullptr;
		bool (*OnLoad)(const HostApi* hostApi) = nullptr;
		void (*OnUnload)() = nullptr;
	};

	inline bool HasCapability(std::uint64_t capabilities, PluginCapabilityFlags capability)
	{
		return (capabilities & static_cast<std::uint64_t>(capability)) != 0;
	}

	using PluginGetApiFn = bool(*)(std::uint32_t engineSdkMajor, std::uint32_t engineSdkMinor, PluginApi* outApi);
}
