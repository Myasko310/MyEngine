#pragma once

#include <algorithm>
#include <chrono>
#include <vector>
#include <string>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <pdh.h>
#include <pdhmsg.h>
#ifdef _MSC_VER
#pragma comment(lib, "pdh.lib")
#endif
#endif

namespace MyEngine
{
	// OS utilization counters, sampled once per second while the panel is open.
	class ProcessUsageMonitor
	{
	public:
		ProcessUsageMonitor() = default;
		ProcessUsageMonitor(const ProcessUsageMonitor&) = delete;
		ProcessUsageMonitor& operator=(const ProcessUsageMonitor&) = delete;
		~ProcessUsageMonitor()
		{
#ifdef _WIN32
			if (m_Query) PdhCloseQuery(m_Query);
#endif
		}

		void Update()
		{
			const auto now = std::chrono::steady_clock::now();
			if (m_Started && now - m_LastSample < std::chrono::seconds(1)) return;
			const double seconds = std::chrono::duration<double>(now - m_LastSample).count();
			m_LastSample = now;
#ifdef _WIN32
			FILETIME creation{}, exit{}, kernel{}, user{};
			cpuAvailable = false;
			if (GetProcessTimes(GetCurrentProcess(), &creation, &exit, &kernel, &user))
			{
				const auto ticks = [](FILETIME t) -> unsigned long long
				{
					return (static_cast<unsigned long long>(t.dwHighDateTime) << 32) | t.dwLowDateTime;
				};
				const auto total = ticks(kernel) + ticks(user);
				const DWORD processors = GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
				if (m_CpuPrimed && seconds > 0.0 && processors > 0 && total >= m_LastCpuTicks)
				{
					cpuPercent = static_cast<float>(std::clamp(
						(total - m_LastCpuTicks) / (seconds * 10000000.0 * processors) * 100.0, 0.0, 100.0));
					cpuAvailable = true;
				}
				m_LastCpuTicks = total;
				m_CpuPrimed = true;
			}
			else m_CpuPrimed = false;

			if (!m_Started)
			{
				m_PidPrefix = L"pid_" + std::to_wstring(GetCurrentProcessId()) + L"_";
				if (PdhOpenQueryW(nullptr, 0, &m_Query) == ERROR_SUCCESS)
				{
					if (PdhAddEnglishCounterW(m_Query, L"\\GPU Engine(*)\\Utilization Percentage", 0, &m_GpuCounter) != ERROR_SUCCESS)
					{
						PdhCloseQuery(m_Query);
						m_Query = nullptr;
					}
				}
			}
			gpuAvailable = false;
			if (m_Query && PdhCollectQueryData(m_Query) == ERROR_SUCCESS)
			{
				DWORD bytes = 0, count = 0;
				if (PdhGetFormattedCounterArrayW(m_GpuCounter, PDH_FMT_DOUBLE, &bytes, &count, nullptr) == PDH_MORE_DATA)
				{
					// Use aligned storage for both the items and their trailing instance names.
					m_Items.resize((bytes + sizeof(PDH_FMT_COUNTERVALUE_ITEM_W) - 1) / sizeof(PDH_FMT_COUNTERVALUE_ITEM_W));
					if (PdhGetFormattedCounterArrayW(m_GpuCounter, PDH_FMT_DOUBLE, &bytes, &count, m_Items.data()) == ERROR_SUCCESS)
					{
						gpuPercent = 0.0f;
						for (DWORD i = 0; i < count; ++i)
						{
							const auto& item = m_Items[i];
							if (!item.szName || std::wstring(item.szName).compare(0, m_PidPrefix.size(), m_PidPrefix) != 0) continue;
							if (item.FmtValue.CStatus != PDH_CSTATUS_VALID_DATA && item.FmtValue.CStatus != PDH_CSTATUS_NEW_DATA) continue;
							gpuAvailable = true;
							gpuPercent = std::max(gpuPercent, static_cast<float>(std::clamp(item.FmtValue.doubleValue, 0.0, 100.0)));
						}
					}
				}
			}
#endif
			m_Started = true;
		}

		float cpuPercent = 0.0f;
		float gpuPercent = 0.0f;
		bool cpuAvailable = false;
		bool gpuAvailable = false;

	private:
		std::chrono::steady_clock::time_point m_LastSample{};
		bool m_Started = false;
#ifdef _WIN32
		bool m_CpuPrimed = false;
		unsigned long long m_LastCpuTicks = 0;
		PDH_HQUERY m_Query = nullptr;
		PDH_HCOUNTER m_GpuCounter = nullptr;
		std::wstring m_PidPrefix;
		std::vector<PDH_FMT_COUNTERVALUE_ITEM_W> m_Items;
#endif
	};
}
