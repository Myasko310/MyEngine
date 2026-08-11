#pragma once

#include <string>

namespace MyEngine
{
	// Wraps a single OpenAL audio buffer loaded from a WAV file on disk.
	class AudioClip
	{
	public:
		explicit AudioClip(const std::string& path);
		~AudioClip();

		AudioClip(const AudioClip&) = delete;
		AudioClip& operator=(const AudioClip&) = delete;

		AudioClip(AudioClip&& other) noexcept;
		AudioClip& operator=(AudioClip&& other) noexcept;

		unsigned int GetBufferID() const { return m_BufferID; }
		const std::string& GetPath() const { return m_Path; }
		bool IsValid() const { return m_BufferID != 0; }

		float GetDuration() const { return m_Duration; }

	private:
		unsigned int m_BufferID = 0;
		std::string m_Path;
		float m_Duration = 0.0f;

		bool LoadWav(const std::string& path);
	};
}
