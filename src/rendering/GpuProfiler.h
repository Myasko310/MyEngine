#pragma once

#include <glad/glad.h>
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace MyEngine
{
	// Publish only complete frames. Pending query slots are never overwritten.
	class GpuProfiler
	{
	public:
		struct PassResult
		{
			std::string name;
			float ms = 0.0f;
		};

		static GpuProfiler& Get()
		{
			static GpuProfiler instance;
			return instance;
		}
		void SetEnabled(bool enabled) { m_Enabled = enabled; }
		bool IsEnabled() const { return m_Enabled; }
		bool IsSupported() const { return m_Supported; }
		const std::vector<PassResult>& GetResults() const { return m_Results; }
		std::uint64_t GetResultFrame() const { return m_ResultFrame; }
		std::uint64_t GetResultAge() const { return m_Frame - m_ResultFrame; }
		int GetResultSampleCount() const { return m_ResultSampleCount; }
		float GetFrameMs() const { return m_FrameMs; }

		void BeginFrame(int sampleCount)
		{
			++m_Frame;
			m_Active = nullptr;
			if (!m_Checked)
			{
				GLint bits = 0;
				if (glQueryCounter && glGetQueryObjectui64v)
					glGetQueryiv(GL_TIMESTAMP, GL_QUERY_COUNTER_BITS, &bits);
				m_Supported = bits > 0;
				m_Checked = true;
			}
			if (!m_Supported) return;
			for (auto& slot : m_Slots)
			{
				if (!slot.pending) continue;
				bool ready = true;
				for (size_t i = 0; i < slot.used; ++i)
				{
					for (GLuint query : slot.passes[i].queries)
					{
						GLint available = 0;
						glGetQueryObjectiv(query, GL_QUERY_RESULT_AVAILABLE, &available);
						ready = ready && available != 0;
					}
				}
				if (!ready) continue;
				if (slot.frame > m_ResultFrame)
				{
					m_Results.clear();
					for (size_t i = 0; i < slot.used; ++i)
					{
						const auto& pass = slot.passes[i];
						GLuint64 start = 0, end = 0;
						glGetQueryObjectui64v(pass.queries[0], GL_QUERY_RESULT, &start);
						glGetQueryObjectui64v(pass.queries[1], GL_QUERY_RESULT, &end);
						float ms = end >= start ? static_cast<float>((end - start) / 1000000.0) : 0.0f;
						if (pass.name == "GPU Frame") m_FrameMs = ms;
						else m_Results.push_back({ pass.name, ms });
					}
					m_ResultFrame = slot.frame;
					m_ResultSampleCount = slot.sampleCount;
				}
				slot.pending = false;
			}
			if (!m_Enabled) return;
			for (auto& slot : m_Slots)
			{
				if (slot.pending) continue;
				slot.used = 0;
				slot.frame = m_Frame;
				slot.sampleCount = sampleCount;
				m_Active = &slot;
				Begin("GPU Frame");
				break;
			}
		}

		void Begin(const char* name)
		{
			if (!m_Active) return;
			if (m_Active->used == m_Active->passes.size())
			{
				m_Active->passes.emplace_back();
				glGenQueries(2, m_Active->passes.back().queries.data());
			}
			auto& pass = m_Active->passes[m_Active->used++];
			pass.name = name;
			pass.open = true;
			glQueryCounter(pass.queries[0], GL_TIMESTAMP);
		}
		void End(const char* name)
		{
			if (!m_Active) return;
			for (size_t i = m_Active->used; i > 0; --i)
			{
				auto& pass = m_Active->passes[i - 1];
				if (pass.open && pass.name == name)
				{
					glQueryCounter(pass.queries[1], GL_TIMESTAMP);
					pass.open = false;
					return;
				}
			}
		}
		void EndFrame()
		{
			if (!m_Active) return;
			End("GPU Frame");
			m_Active->pending = true;
			m_Active = nullptr;
		}
		// Call while the rendering context is still current.
		void Shutdown()
		{
			for (auto& slot : m_Slots)
			{
				for (auto& pass : slot.passes) glDeleteQueries(2, pass.queries.data());
				slot = Slot{};
			}
			m_Active = nullptr;
			m_Results.clear();
			m_Frame = m_ResultFrame = 0;
			m_FrameMs = 0.0f;
			m_Checked = false;
		}

	private:
		struct Pass
		{
			std::array<GLuint, 2> queries{};
			std::string name;
			bool open = false;
		};
		struct Slot
		{
			std::vector<Pass> passes;
			size_t used = 0;
			std::uint64_t frame = 0;
			int sampleCount = 64;
			bool pending = false;
		};
		std::array<Slot, 4> m_Slots{};
		Slot* m_Active = nullptr;
		std::vector<PassResult> m_Results;
		std::uint64_t m_Frame = 0, m_ResultFrame = 0;
		int m_ResultSampleCount = 64;
		float m_FrameMs = 0.0f;
		bool m_Enabled = true, m_Checked = false, m_Supported = false;
	};

	struct ScopedGpuTimer
	{
		explicit ScopedGpuTimer(const char* name) : m_Name(name) { GpuProfiler::Get().Begin(m_Name); }
		~ScopedGpuTimer() { GpuProfiler::Get().End(m_Name); }
		ScopedGpuTimer(const ScopedGpuTimer&) = delete;
		ScopedGpuTimer& operator=(const ScopedGpuTimer&) = delete;
	private:
		const char* m_Name;
	};
}
