#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace MyEngine
{
	struct AssetMetadata
	{
		std::string path;
		std::string type;
		std::uint64_t sizeBytes = 0;
		std::int64_t lastWriteTicks = 0;
		std::vector<std::string> dependencies;
	};

	class AssetPipeline
	{
	public:
		static AssetPipeline& Get();

		void Start();
		void Stop();
		void Tick();

		void ScanAssets(const std::string& rootPath);
		bool QueueImport(const std::string& assetPath);
		int ClearQueuedImports();
		bool SaveDependencyCache(const std::string& cachePath) const;
		bool LoadDependencyCache(const std::string& cachePath);
		bool InvalidateAssetMetadata(const std::string& assetPath);

		std::vector<AssetMetadata> GetMetadataSnapshot() const;
		std::vector<std::string> GetRecentEvents() const;
		std::vector<std::string> GetDependentsForAsset(const std::string& assetPath) const;
		int QueueReimportDependents(const std::string& assetPath);

		int GetPendingJobCount() const;
		int GetCompletedJobCount() const;
		int GetFailedJobCount() const;
		int GetDeduplicatedJobCount() const;

	private:
		AssetPipeline() = default;
		~AssetPipeline();
		AssetPipeline(const AssetPipeline&) = delete;
		AssetPipeline& operator=(const AssetPipeline&) = delete;

		void WorkerLoop();
		void ExecuteImport(const std::string& assetPath);
		void AddEvent(const std::string& text);
		AssetMetadata BuildMetadataForPath(const std::string& path) const;
		std::vector<std::string> ExtractDependencies(const std::string& path) const;
		std::string DetectType(const std::string& path) const;

		mutable std::mutex m_MetadataMutex;
		std::unordered_map<std::string, AssetMetadata> m_MetadataByPath;

		mutable std::mutex m_EventMutex;
		std::vector<std::string> m_RecentEvents;

		mutable std::mutex m_QueueMutex;
		std::condition_variable m_QueueCondition;
		std::queue<std::string> m_PendingImports;
		std::unordered_map<std::string, bool> m_QueuedOrInFlight;

		std::thread m_Worker;
		std::string m_LastScannedRootPath;
		std::unordered_map<std::string, std::int64_t> m_LastObservedWriteTicks;
		std::atomic<bool> m_Running{ false };
		std::atomic<int> m_PendingCount{ 0 };
		std::atomic<int> m_CompletedCount{ 0 };
		std::atomic<int> m_FailedCount{ 0 };
		std::atomic<int> m_DeduplicatedCount{ 0 };
	};
}
