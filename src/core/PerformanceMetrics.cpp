#include "core/PerformanceMetrics.h"
#include <iostream>
#include <iomanip>

namespace MyEngine::Performance
{
	std::map<std::string, PerformanceMetrics::TimingEntry>& PerformanceMetrics::GetTimingMap()
	{
		static std::map<std::string, TimingEntry> timingMap;
		return timingMap;
	}

	float PerformanceMetrics::GetElapsedMs(std::chrono::high_resolution_clock::time_point start)
	{
		auto end = std::chrono::high_resolution_clock::now();
		auto duration = std::chrono::duration<float, std::milli>(end - start);
		return duration.count();
	}

	void PerformanceMetrics::StartMeasure(const std::string& name)
	{
		auto& timingMap = GetTimingMap();
		if (timingMap.find(name) == timingMap.end())
		{
			timingMap[name] = TimingEntry{ name, {}, {} };
		}
		timingMap[name].startTime = std::chrono::high_resolution_clock::now();
	}

	void PerformanceMetrics::EndMeasure(const std::string& name)
	{
		auto& timingMap = GetTimingMap();
		auto it = timingMap.find(name);
		if (it == timingMap.end())
			return;

		float elapsedMs = GetElapsedMs(it->second.startTime);
		auto& samples = it->second.samples;

		samples.push_back(elapsedMs);

		// Keep only the last MAX_SAMPLES measurements
		if (samples.size() > TimingEntry::MAX_SAMPLES)
		{
			samples.erase(samples.begin(), samples.begin() + (samples.size() - TimingEntry::MAX_SAMPLES));
		}
	}

	void PerformanceMetrics::Clear()
	{
		GetTimingMap().clear();
	}

	PerformanceMetrics::FrameStat PerformanceMetrics::GetStat(const std::string& name)
	{
		auto& timingMap = GetTimingMap();
		auto it = timingMap.find(name);

		FrameStat stat;
		stat.name = name;

		if (it == timingMap.end() || it->second.samples.empty())
			return stat;

		const auto& samples = it->second.samples;
		stat.sampleCount = static_cast<uint32_t>(samples.size());

		float sum = 0.0f;
		stat.minMs = samples[0];
		stat.maxMs = samples[0];

		for (float sample : samples)
		{
			sum += sample;
			stat.minMs = std::min(stat.minMs, sample);
			stat.maxMs = std::max(stat.maxMs, sample);
		}

		stat.avgMs = sum / samples.size();
		return stat;
	}

	std::vector<PerformanceMetrics::FrameStat> PerformanceMetrics::GetAllStats()
	{
		std::vector<FrameStat> stats;
		auto& timingMap = GetTimingMap();

		for (const auto& [name, entry] : timingMap)
		{
			stats.push_back(GetStat(name));
		}

		// Sort by average time (descending)
		std::sort(stats.begin(), stats.end(),
			[](const FrameStat& a, const FrameStat& b)
			{
				return a.avgMs > b.avgMs;
			});

		return stats;
	}

	void PerformanceMetrics::PrintStats()
	{
		auto stats = GetAllStats();

		if (stats.empty())
		{
			std::cout << "No performance measurements recorded." << std::endl;
			return;
		}

		std::cout << "\n=== Performance Metrics ===" << std::endl;
		std::cout << std::left << std::setw(25) << "Measurement"
			<< std::setw(10) << "Avg (ms)"
			<< std::setw(10) << "Min (ms)"
			<< std::setw(10) << "Max (ms)"
			<< std::setw(10) << "Samples" << std::endl;
		std::cout << std::string(65, '-') << std::endl;

		for (const auto& stat : stats)
		{
			std::cout << std::left
				<< std::setw(25) << stat.name
				<< std::setw(10) << std::fixed << std::setprecision(3) << stat.avgMs
				<< std::setw(10) << std::fixed << std::setprecision(3) << stat.minMs
				<< std::setw(10) << std::fixed << std::setprecision(3) << stat.maxMs
				<< std::setw(10) << stat.sampleCount << std::endl;
		}
		std::cout << std::string(65, '-') << std::endl;
	}
}
