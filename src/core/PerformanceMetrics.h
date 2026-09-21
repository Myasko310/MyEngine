#pragma once

#include <chrono>
#include <map>
#include <string>
#include <vector>
#include <algorithm>

namespace MyEngine::Performance
{
	// ============================================================
	// PerformanceMetrics
	// ============================================================
	// Lightweight profiling system for measuring frame-time
	// of hot paths (rendering, physics, animation, etc.)
	//
	// Usage:
	//   PerformanceMetrics::StartMeasure("Physics");
	//   // ... do physics work ...
	//   PerformanceMetrics::EndMeasure("Physics");
	//
	//   for (const auto& stat : PerformanceMetrics::GetAllStats()) {
	//       cout << stat.name << ": " << stat.avgMs << " ms" << endl;
	//   }
	// ============================================================
	class PerformanceMetrics
	{
	public:
		struct FrameStat
		{
			std::string name;
			float minMs = 0.0f;
			float maxMs = 0.0f;
			float avgMs = 0.0f;
			uint32_t sampleCount = 0;
		};

		// Start measuring a named section
		static void StartMeasure(const std::string& name);

		// End measuring a named section
		static void EndMeasure(const std::string& name);

		// Clear all statistics
		static void Clear();

		// Get statistics for a specific measurement
		static FrameStat GetStat(const std::string& name);

		// Get all statistics
		static std::vector<FrameStat> GetAllStats();

		// Print statistics to console
		static void PrintStats();

	private:
		struct TimingEntry
		{
			std::string name;
			std::chrono::high_resolution_clock::time_point startTime;
			std::vector<float> samples; // milliseconds
			static const size_t MAX_SAMPLES = 300; // ~5 seconds at 60 FPS
		};

		static std::map<std::string, TimingEntry>& GetTimingMap();
		static float GetElapsedMs(std::chrono::high_resolution_clock::time_point start);
	};

	// ============================================================
	// ScopedTimer - RAII helper for automatic timing
	// ============================================================
	class ScopedTimer
	{
	public:
		explicit ScopedTimer(const std::string& name)
			: m_Name(name)
		{
			PerformanceMetrics::StartMeasure(m_Name);
		}

		~ScopedTimer()
		{
			PerformanceMetrics::EndMeasure(m_Name);
		}

	private:
		std::string m_Name;
	};
}

// Convenient macro for scoped timing
#define PROFILE_SCOPE(name) MyEngine::Performance::ScopedTimer _profiler(name)
