#include "core/AssetPipeline.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <regex>

#include "animation/AnimationStateMachine.h"
#include "core/AssetManager.h"

namespace fs = std::filesystem;

namespace
{
	std::string ToLower(std::string value)
	{
		std::transform(value.begin(), value.end(), value.begin(),
			[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		return value;
	}

	std::string NormalizePathKey(const std::string& path)
	{
		fs::path normalized = fs::path(path).lexically_normal();
		std::string result = normalized.generic_string();
#ifdef _WIN32
		result = ToLower(result);
#endif
		return result;
	}

	bool EndsWith(const std::string& value, const std::string& suffix)
	{
		if (suffix.size() > value.size())
			return false;
		return std::equal(suffix.rbegin(), suffix.rend(), value.rbegin());
	}
}

namespace MyEngine
{
	AssetPipeline& AssetPipeline::Get()
	{
		static AssetPipeline instance;
		return instance;
	}

	AssetPipeline::~AssetPipeline()
	{
		Stop();
	}

	void AssetPipeline::Start()
	{
		if (m_Running.exchange(true))
			return;

		m_Worker = std::thread([this]() { WorkerLoop(); });
		AddEvent("Asset pipeline worker started.");
	}

	void AssetPipeline::Stop()
	{
		if (!m_Running.exchange(false))
			return;

		m_QueueCondition.notify_all();
		if (m_Worker.joinable())
			m_Worker.join();

		AddEvent("Asset pipeline worker stopped.");
	}

	void AssetPipeline::Tick()
	{
		if (!m_Running)
			Start();
	}

	void AssetPipeline::ScanAssets(const std::string& rootPath)
	{
		if (rootPath.empty() || !fs::exists(rootPath))
			return;

		std::unordered_map<std::string, AssetMetadata> scanned;
		for (const auto& entry : fs::recursive_directory_iterator(rootPath))
		{
			if (!entry.is_regular_file())
				continue;
			const std::string path = entry.path().generic_string();
			scanned[path] = BuildMetadataForPath(path);
		}

		{
			std::lock_guard<std::mutex> lock(m_MetadataMutex);
			m_MetadataByPath.swap(scanned);
		}

		AddEvent("Asset scan completed: " + std::to_string(m_MetadataByPath.size()) + " files indexed.");
	}

	bool AssetPipeline::QueueImport(const std::string& assetPath)
	{
		if (assetPath.empty())
			return false;

		Start();
		const std::string key = NormalizePathKey(assetPath);
		{
			std::lock_guard<std::mutex> lock(m_QueueMutex);
			if (m_QueuedOrInFlight.find(key) != m_QueuedOrInFlight.end())
			{
				++m_DeduplicatedCount;
				AddEvent("Skipped duplicate import: " + assetPath);
				return false;
			}

			m_PendingImports.push(assetPath);
			m_QueuedOrInFlight[key] = true;
			++m_PendingCount;
		}
		m_QueueCondition.notify_one();
		AddEvent("Queued import: " + assetPath);
		return true;
	}

	int AssetPipeline::ClearQueuedImports()
	{
		std::lock_guard<std::mutex> lock(m_QueueMutex);
		int cleared = 0;
		while (!m_PendingImports.empty())
		{
			m_QueuedOrInFlight.erase(NormalizePathKey(m_PendingImports.front()));
			m_PendingImports.pop();
			++cleared;
		}
		m_PendingCount = std::max(0, m_PendingCount.load() - cleared);
		if (cleared > 0)
			AddEvent("Cleared " + std::to_string(cleared) + " queued import(s).");
		return cleared;
	}

	std::vector<AssetMetadata> AssetPipeline::GetMetadataSnapshot() const
	{
		std::vector<AssetMetadata> snapshot;
		std::lock_guard<std::mutex> lock(m_MetadataMutex);
		snapshot.reserve(m_MetadataByPath.size());
		for (const auto& [_, metadata] : m_MetadataByPath)
			snapshot.push_back(metadata);
		std::sort(snapshot.begin(), snapshot.end(),
			[](const AssetMetadata& a, const AssetMetadata& b) { return a.path < b.path; });
		return snapshot;
	}

	std::vector<std::string> AssetPipeline::GetRecentEvents() const
	{
		std::lock_guard<std::mutex> lock(m_EventMutex);
		return m_RecentEvents;
	}

	int AssetPipeline::GetPendingJobCount() const
	{
		return m_PendingCount.load();
	}

	int AssetPipeline::GetCompletedJobCount() const
	{
		return m_CompletedCount.load();
	}

	int AssetPipeline::GetFailedJobCount() const
	{
		return m_FailedCount.load();
	}

	int AssetPipeline::GetDeduplicatedJobCount() const
	{
		return m_DeduplicatedCount.load();
	}

	void AssetPipeline::WorkerLoop()
	{
		while (m_Running)
		{
			std::string nextPath;
			std::string nextKey;
			{
				std::unique_lock<std::mutex> lock(m_QueueMutex);
				m_QueueCondition.wait(lock, [this]() { return !m_Running || !m_PendingImports.empty(); });
				if (!m_Running)
					break;

				nextPath = m_PendingImports.front();
				nextKey = NormalizePathKey(nextPath);
				m_PendingImports.pop();
			}

			try
			{
				ExecuteImport(nextPath);
				++m_CompletedCount;
				AddEvent("Imported: " + nextPath);
			}
			catch (...)
			{
				++m_FailedCount;
				AddEvent("Import failed: " + nextPath);
			}

			{
				std::lock_guard<std::mutex> lock(m_QueueMutex);
				m_QueuedOrInFlight.erase(nextKey);
			}
			--m_PendingCount;
		}
	}

	void AssetPipeline::ExecuteImport(const std::string& assetPath)
	{
		const std::string lowerPath = ToLower(assetPath);
		if (EndsWith(lowerPath, ".obj") || EndsWith(lowerPath, ".fbx") || EndsWith(lowerPath, ".gltf") || EndsWith(lowerPath, ".glb") || EndsWith(lowerPath, ".dae"))
		{
			AssetManager::LoadModel(assetPath);
			AssetManager::ImportModelMaterials(assetPath);
		}
		else if (EndsWith(lowerPath, ".png") || EndsWith(lowerPath, ".jpg") || EndsWith(lowerPath, ".jpeg") || EndsWith(lowerPath, ".bmp") || EndsWith(lowerPath, ".tga"))
		{
			AssetManager::LoadTexture(assetPath, true);
		}
		else if (EndsWith(lowerPath, ".wav"))
		{
			AssetManager::LoadAudioClip(assetPath);
		}
		else if (lowerPath.find(".material.json") != std::string::npos)
		{
			AssetManager::LoadMaterial(assetPath);
		}
		else if (lowerPath.find(".animstate.json") != std::string::npos)
		{
			AnimationStateMachine sm;
			sm.LoadFromFile(assetPath);
		}
	}

	void AssetPipeline::AddEvent(const std::string& text)
	{
		std::lock_guard<std::mutex> lock(m_EventMutex);
		m_RecentEvents.push_back(text);
		if (m_RecentEvents.size() > 40)
			m_RecentEvents.erase(m_RecentEvents.begin(), m_RecentEvents.begin() + static_cast<std::ptrdiff_t>(m_RecentEvents.size() - 40));
	}

	AssetMetadata AssetPipeline::BuildMetadataForPath(const std::string& path) const
	{
		AssetMetadata metadata;
		metadata.path = path;
		metadata.type = DetectType(path);
		metadata.dependencies = ExtractDependencies(path);

		std::error_code ec;
		metadata.sizeBytes = static_cast<std::uint64_t>(fs::file_size(path, ec));
		ec.clear();
		auto time = fs::last_write_time(path, ec);
		if (!ec)
		{
			auto ticks = time.time_since_epoch().count();
			metadata.lastWriteTicks = static_cast<std::int64_t>(ticks);
		}
		return metadata;
	}

	std::vector<std::string> AssetPipeline::ExtractDependencies(const std::string& path) const
	{
		std::vector<std::string> deps;
		std::ifstream file(path, std::ios::binary);
		if (!file)
			return deps;

		std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
		if (content.empty())
			return deps;

		static const std::regex quotedPathRegex("\"([^\"]+\\.(?:obj|fbx|gltf|glb|dae|png|jpg|jpeg|bmp|tga|wav|lua|json))\"", std::regex::icase);
		for (std::sregex_iterator it(content.begin(), content.end(), quotedPathRegex), end; it != end; ++it)
		{
			if (it->size() < 2)
				continue;
			std::string value = (*it)[1].str();
			if (std::find(deps.begin(), deps.end(), value) == deps.end())
				deps.push_back(value);
		}

		return deps;
	}

	std::string AssetPipeline::DetectType(const std::string& path) const
	{
		const std::string lowerPath = ToLower(path);
		if (lowerPath.find(".material.json") != std::string::npos)
			return "material";
		if (lowerPath.find(".prefab.json") != std::string::npos)
			return "prefab";
		if (lowerPath.find(".animstate.json") != std::string::npos)
			return "animstate";
		if (EndsWith(lowerPath, ".json"))
			return "scene";
		if (EndsWith(lowerPath, ".obj") || EndsWith(lowerPath, ".fbx") || EndsWith(lowerPath, ".gltf") || EndsWith(lowerPath, ".glb") || EndsWith(lowerPath, ".dae"))
			return "model";
		if (EndsWith(lowerPath, ".png") || EndsWith(lowerPath, ".jpg") || EndsWith(lowerPath, ".jpeg") || EndsWith(lowerPath, ".bmp") || EndsWith(lowerPath, ".tga"))
			return "texture";
		if (EndsWith(lowerPath, ".wav"))
			return "audio";
		if (EndsWith(lowerPath, ".lua"))
			return "script";
		return "file";
	}
}
