#include "plugins/PluginManager.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#endif

namespace
{
	std::string Trim(const std::string& text)
	{
		const auto first = std::find_if_not(text.begin(), text.end(), [](unsigned char c) { return std::isspace(c) != 0; });
		const auto last = std::find_if_not(text.rbegin(), text.rend(), [](unsigned char c) { return std::isspace(c) != 0; }).base();
		if (first >= last)
			return std::string();
		return std::string(first, last);
	}

	struct PluginLoadContext
	{
		const MyEngine::Plugins::PluginApi* api = nullptr;
		const MyEngine::Plugins::HostApi* hostApi = nullptr;
		bool attemptedNetHookRegistration = false;
		bool netHookRegistrationAccepted = false;
	};

	thread_local PluginLoadContext* g_ActivePluginLoadContext = nullptr;

	bool RegisterNetReplicationHooksBridge(const char* pluginName)
	{
		if (!g_ActivePluginLoadContext || !g_ActivePluginLoadContext->api || !g_ActivePluginLoadContext->hostApi)
			return false;

		g_ActivePluginLoadContext->attemptedNetHookRegistration = true;
		const auto& api = *g_ActivePluginLoadContext->api;
		if (!MyEngine::Plugins::HasCapability(api.declaredCapabilities, MyEngine::Plugins::PluginCapability_NetReplicationHooks))
		{
			g_ActivePluginLoadContext->netHookRegistrationAccepted = false;
			return false;
		}

		if (!g_ActivePluginLoadContext->hostApi->RegisterNetReplicationHooks)
		{
			g_ActivePluginLoadContext->netHookRegistrationAccepted = false;
			return false;
		}

		const char* requestedName = (pluginName && pluginName[0] != '\0') ? pluginName : api.pluginName;
		g_ActivePluginLoadContext->netHookRegistrationAccepted = g_ActivePluginLoadContext->hostApi->RegisterNetReplicationHooks(requestedName);
		return g_ActivePluginLoadContext->netHookRegistrationAccepted;
	}
}

namespace MyEngine::Plugins
{
	bool PluginManager::ParseManifestFile(const std::filesystem::path& manifestPath, PluginPackageManifest& outManifest, std::string& outError)
	{
		std::ifstream ifs(manifestPath);
		if (!ifs)
		{
			outError = "Failed to open manifest: " + manifestPath.string();
			return false;
		}

		std::string packageName;
		std::string dllRelativePath;
		std::string sdkMajorText;
		std::string sdkMinorText;

		std::string line;
		while (std::getline(ifs, line))
		{
			const std::string trimmed = Trim(line);
			if (trimmed.empty() || trimmed[0] == '#' || trimmed[0] == ';')
				continue;

			const size_t eq = trimmed.find('=');
			if (eq == std::string::npos)
				continue;

			const std::string key = Trim(trimmed.substr(0, eq));
			const std::string value = Trim(trimmed.substr(eq + 1));
			if (key == "name")
				packageName = value;
			else if (key == "dll")
				dllRelativePath = value;
			else if (key == "sdk_major")
				sdkMajorText = value;
			else if (key == "sdk_minor")
				sdkMinorText = value;
		}

		if (packageName.empty())
		{
			outError = "Manifest missing required key 'name': " + manifestPath.string();
			return false;
		}
		if (dllRelativePath.empty())
		{
			outError = "Manifest missing required key 'dll': " + manifestPath.string();
			return false;
		}
		if (sdkMajorText.empty() || sdkMinorText.empty())
		{
			outError = "Manifest missing required sdk version keys: " + manifestPath.string();
			return false;
		}

		try
		{
			outManifest.sdkVersionMajor = static_cast<std::uint32_t>(std::stoul(sdkMajorText));
			outManifest.sdkVersionMinor = static_cast<std::uint32_t>(std::stoul(sdkMinorText));
		}
		catch (...)
		{
			outError = "Manifest has invalid sdk version numbers: " + manifestPath.string();
			return false;
		}

		outManifest.packageName = packageName;
		outManifest.manifestPath = manifestPath;
		outManifest.packageDirectory = manifestPath.parent_path();
		outManifest.binaryPath = outManifest.packageDirectory / dllRelativePath;
		return true;
	}

	std::vector<PluginPackageManifest> PluginManager::DiscoverPluginPackages(const std::filesystem::path& pluginsRoot, std::vector<std::string>* outWarnings)
	{
		std::vector<PluginPackageManifest> manifests;
		if (!std::filesystem::exists(pluginsRoot) || !std::filesystem::is_directory(pluginsRoot))
			return manifests;

		for (const auto& entry : std::filesystem::directory_iterator(pluginsRoot))
		{
			if (!entry.is_directory())
				continue;

			const std::filesystem::path manifestPath = entry.path() / "plugin.ini";
			if (!std::filesystem::exists(manifestPath))
				continue;

			PluginPackageManifest manifest;
			std::string error;
			if (!ParseManifestFile(manifestPath, manifest, error))
			{
				if (outWarnings)
					outWarnings->push_back(error);
				continue;
			}

			manifests.push_back(std::move(manifest));
		}

		return manifests;
	}

	bool PluginManager::IsManifestCompatible(const PluginPackageManifest& manifest, std::string& outReason)
	{
		if (manifest.sdkVersionMajor != kPluginSdkVersionMajor)
		{
			outReason = "SDK major mismatch (plugin=" + std::to_string(manifest.sdkVersionMajor) + ", engine=" + std::to_string(kPluginSdkVersionMajor) + ")";
			return false;
		}

		if (manifest.sdkVersionMinor > kPluginSdkVersionMinor)
		{
			outReason = "SDK minor too new (plugin=" + std::to_string(manifest.sdkVersionMinor) + ", engine=" + std::to_string(kPluginSdkVersionMinor) + ")";
			return false;
		}

		outReason.clear();
		return true;
	}

	bool PluginManager::CanRegisterNetReplicationHooks(const PluginApi& api)
	{
		return HasCapability(api.declaredCapabilities, PluginCapability_NetReplicationHooks);
	}

	bool PluginManager::LoadPlugin(const PluginPackageManifest& manifest, const HostApi& hostApi)
	{
		const auto existing = std::find_if(m_LoadedPlugins.begin(), m_LoadedPlugins.end(), [&](const LoadedPlugin& loaded)
		{
			return loaded.manifest.packageDirectory == manifest.packageDirectory;
		});
		if (existing != m_LoadedPlugins.end())
			return true;

		std::string compatibilityReason;
		if (!IsManifestCompatible(manifest, compatibilityReason))
		{
			m_LastErrors.push_back("Plugin '" + manifest.packageName + "' skipped: " + compatibilityReason);
			return false;
		}

#ifdef _WIN32
		if (!std::filesystem::exists(manifest.binaryPath))
		{
			m_LastErrors.push_back("Plugin binary missing: " + manifest.binaryPath.string());
			return false;
		}

		HMODULE module = LoadLibraryW(manifest.binaryPath.c_str());
		if (!module)
		{
			m_LastErrors.push_back("LoadLibrary failed for plugin: " + manifest.binaryPath.string());
			return false;
		}

		auto getApi = reinterpret_cast<PluginGetApiFn>(GetProcAddress(module, "MyEnginePlugin_GetApi"));
		if (!getApi)
		{
			m_LastErrors.push_back("Plugin entrypoint 'MyEnginePlugin_GetApi' missing: " + manifest.binaryPath.string());
			FreeLibrary(module);
			return false;
		}

		PluginApi api{};
		if (!getApi(kPluginSdkVersionMajor, kPluginSdkVersionMinor, &api))
		{
			m_LastErrors.push_back("Plugin API negotiation failed: " + manifest.binaryPath.string());
			FreeLibrary(module);
			return false;
		}

		if (api.sdkVersionMajor != kPluginSdkVersionMajor || api.sdkVersionMinor > kPluginSdkVersionMinor)
		{
			m_LastErrors.push_back("Plugin exported incompatible SDK version: " + manifest.binaryPath.string());
			FreeLibrary(module);
			return false;
		}

		if (!api.pluginName || api.pluginName[0] == '\0')
		{
			m_LastErrors.push_back("Plugin exported empty pluginName metadata: " + manifest.binaryPath.string());
			FreeLibrary(module);
			return false;
		}

		if (api.declaredCapabilities == PluginCapability_None)
		{
			m_LastErrors.push_back("Plugin declared no capabilities: " + manifest.binaryPath.string());
			FreeLibrary(module);
			return false;
		}

		if (!HasCapability(api.declaredCapabilities, PluginCapability_LifecycleHooks))
		{
			m_LastErrors.push_back("Plugin missing required LifecycleHooks capability: " + manifest.binaryPath.string());
			FreeLibrary(module);
			return false;
		}

		if (!api.OnLoad || !api.OnUnload)
		{
			m_LastErrors.push_back("Plugin missing required lifecycle callbacks: " + manifest.binaryPath.string());
			FreeLibrary(module);
			return false;
		}

		if (api.pluginVersionMajor == 0u && api.pluginVersionMinor == 0u && api.pluginVersionPatch == 0u)
		{
			m_LastErrors.push_back("Plugin semantic version metadata is missing (0.0.0): " + manifest.binaryPath.string());
			FreeLibrary(module);
			return false;
		}

		LoadedPlugin loaded;
		loaded.manifest = manifest;
		loaded.api = api;
		loaded.hostApi = hostApi;
		loaded.hostApi.RegisterNetReplicationHooks = &RegisterNetReplicationHooksBridge;
		loaded.moduleHandle = module;

		m_LoadedPlugins.push_back(std::move(loaded));
		LoadedPlugin& loadedPlugin = m_LoadedPlugins.back();

		PluginLoadContext loadContext;
		loadContext.api = &loadedPlugin.api;
		loadContext.hostApi = &hostApi;
		g_ActivePluginLoadContext = &loadContext;

		const bool onLoadOk = loadedPlugin.api.OnLoad(&loadedPlugin.hostApi);
		g_ActivePluginLoadContext = nullptr;
		if (!onLoadOk)
		{
			m_LastErrors.push_back("Plugin OnLoad failed: " + manifest.binaryPath.string());
			m_LoadedPlugins.pop_back();
			FreeLibrary(module);
			return false;
		}

		if (loadContext.attemptedNetHookRegistration && !loadContext.netHookRegistrationAccepted)
		{
			m_LastErrors.push_back("Plugin attempted net hook registration without capability or host approval: " + manifest.binaryPath.string());
			m_LoadedPlugins.pop_back();
			FreeLibrary(module);
			return false;
		}

		return true;
#else
		(void)manifest;
		(void)hostApi;
		m_LastErrors.push_back("Dynamic plugin loading is currently implemented for Windows only.");
		return false;
#endif
	}

	bool PluginManager::LoadAllFromRoot(const std::filesystem::path& pluginsRoot, const HostApi& hostApi)
	{
		m_LastErrors.clear();
		std::vector<std::string> warnings;
		const auto manifests = DiscoverPluginPackages(pluginsRoot, &warnings);
		m_LastErrors.insert(m_LastErrors.end(), warnings.begin(), warnings.end());

		bool loadedAny = false;
		for (const auto& manifest : manifests)
			loadedAny = LoadPlugin(manifest, hostApi) || loadedAny;

		return loadedAny;
	}

	void PluginManager::UnloadAll()
	{
#ifdef _WIN32
		for (auto it = m_LoadedPlugins.rbegin(); it != m_LoadedPlugins.rend(); ++it)
		{
			if (it->api.OnUnload)
				it->api.OnUnload();
			if (it->moduleHandle)
				FreeLibrary(static_cast<HMODULE>(it->moduleHandle));
			it->moduleHandle = nullptr;
		}
#endif
		m_LoadedPlugins.clear();
	}

	bool PluginManager::ReloadAllFromRoot(const std::filesystem::path& pluginsRoot, const HostApi& hostApi)
	{
		UnloadAll();
		return LoadAllFromRoot(pluginsRoot, hostApi);
	}

	const std::vector<LoadedPlugin>& PluginManager::GetLoadedPlugins() const
	{
		return m_LoadedPlugins;
	}

	const std::vector<std::string>& PluginManager::GetLastErrors() const
	{
		return m_LastErrors;
	}
}
