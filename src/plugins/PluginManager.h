#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "plugins/PluginSDK.h"

namespace MyEngine::Plugins
{
	struct PluginPackageManifest
	{
		std::string packageName;
		std::filesystem::path packageDirectory;
		std::filesystem::path manifestPath;
		std::filesystem::path binaryPath;
		std::uint32_t sdkVersionMajor = 0;
		std::uint32_t sdkVersionMinor = 0;
	};

	struct LoadedPlugin
	{
		PluginPackageManifest manifest;
		PluginApi api{};
		HostApi hostApi{};
		void* moduleHandle = nullptr;
	};

	class PluginManager
	{
	public:
		static bool ParseManifestFile(const std::filesystem::path& manifestPath, PluginPackageManifest& outManifest, std::string& outError);
		static std::vector<PluginPackageManifest> DiscoverPluginPackages(const std::filesystem::path& pluginsRoot, std::vector<std::string>* outWarnings = nullptr);
		static bool IsManifestCompatible(const PluginPackageManifest& manifest, std::string& outReason);
		static bool CanRegisterNetReplicationHooks(const PluginApi& api);

		bool LoadAllFromRoot(const std::filesystem::path& pluginsRoot, const HostApi& hostApi);
		bool ReloadAllFromRoot(const std::filesystem::path& pluginsRoot, const HostApi& hostApi);
		void UnloadAll();

		const std::vector<LoadedPlugin>& GetLoadedPlugins() const;
		const std::vector<std::string>& GetLastErrors() const;

	private:
		bool LoadPlugin(const PluginPackageManifest& manifest, const HostApi& hostApi);

		std::vector<LoadedPlugin> m_LoadedPlugins;
		std::vector<std::string> m_LastErrors;
	};
}
