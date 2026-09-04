#include "audio/AudioClip.h"

#include <AL/al.h>

#include <fstream>
#include <iostream>
#include <cstring>
#include <utility>
#include <vector>

namespace MyEngine
{
	namespace
	{
		// Minimal RIFF/WAV parser supporting PCM (8/16-bit, mono/stereo).
		struct WavData
		{
			int channels = 0;
			int sampleRate = 0;
			int bitsPerSample = 0;
			std::vector<char> pcmData;
		};

		bool ParseWav(const std::string& path, WavData& out)
		{
			std::ifstream file(path, std::ios::binary);
			if (!file)
			{
				std::cerr << "[AudioClip] Failed to open WAV file: " << path << std::endl;
				return false;
			}

			char riffHeader[4];
			file.read(riffHeader, 4);
			if (std::strncmp(riffHeader, "RIFF", 4) != 0)
			{
				std::cerr << "[AudioClip] Not a RIFF file: " << path << std::endl;
				return false;
			}

			file.ignore(4); // chunk size, unused
			char waveHeader[4];
			file.read(waveHeader, 4);
			if (std::strncmp(waveHeader, "WAVE", 4) != 0)
			{
				std::cerr << "[AudioClip] Not a WAVE file: " << path << std::endl;
				return false;
			}

			bool haveFmt = false;
			bool haveData = false;

			while (file && !(haveFmt && haveData))
			{
				char chunkId[4];
				uint32_t chunkSize = 0;
				file.read(chunkId, 4);
				file.read(reinterpret_cast<char*>(&chunkSize), 4);

				if (!file)
					break;

				if (std::strncmp(chunkId, "fmt ", 4) == 0)
				{
					uint16_t audioFormat = 0, numChannels = 0, bitsPerSample = 0;
					uint32_t sampleRate = 0, byteRate = 0;
					uint16_t blockAlign = 0;

					file.read(reinterpret_cast<char*>(&audioFormat), 2);
					file.read(reinterpret_cast<char*>(&numChannels), 2);
					file.read(reinterpret_cast<char*>(&sampleRate), 4);
					file.read(reinterpret_cast<char*>(&byteRate), 4);
					file.read(reinterpret_cast<char*>(&blockAlign), 2);
					file.read(reinterpret_cast<char*>(&bitsPerSample), 2);

					// Skip any extra fmt bytes beyond the standard 16
					if (chunkSize > 16)
						file.ignore(chunkSize - 16);

					if (audioFormat != 1 /* PCM */)
					{
						std::cerr << "[AudioClip] Unsupported WAV format (only PCM is supported): " << path << std::endl;
						return false;
					}

					out.channels = numChannels;
					out.sampleRate = static_cast<int>(sampleRate);
					out.bitsPerSample = bitsPerSample;
					haveFmt = true;
				}
				else if (std::strncmp(chunkId, "data", 4) == 0)
				{
					out.pcmData.resize(chunkSize);
					file.read(out.pcmData.data(), chunkSize);
					haveData = true;
				}
				else
				{
					// Skip unknown chunk (e.g. "LIST", "fact")
					file.ignore(chunkSize);
				}

				// WAV chunks are word-aligned; skip a pad byte if size is odd
				if (chunkSize % 2 != 0)
					file.ignore(1);
			}

			if (!haveFmt || !haveData)
			{
				std::cerr << "[AudioClip] Missing fmt or data chunk in WAV file: " << path << std::endl;
				return false;
			}

			return true;
		}
	}

	AudioClip::AudioClip(const std::string& path)
		: m_Path(path)
	{
		LoadWav(path);
	}

	AudioClip::~AudioClip()
	{
		if (m_BufferID != 0)
		{
			alDeleteBuffers(1, &m_BufferID);
			m_BufferID = 0;
		}
	}

	AudioClip::AudioClip(AudioClip&& other) noexcept
		: m_BufferID(other.m_BufferID), m_Path(std::move(other.m_Path)), m_Duration(other.m_Duration)
	{
		other.m_BufferID = 0;
	}

	AudioClip& AudioClip::operator=(AudioClip&& other) noexcept
	{
		if (this != &other)
		{
			if (m_BufferID != 0)
				alDeleteBuffers(1, &m_BufferID);

			m_BufferID = other.m_BufferID;
			m_Path = std::move(other.m_Path);
			m_Duration = other.m_Duration;
			other.m_BufferID = 0;
		}
		return *this;
	}

	bool AudioClip::LoadWav(const std::string& path)
	{
		WavData wav;
		if (!ParseWav(path, wav))
			return false;

		ALenum format;
		if (wav.channels == 1 && wav.bitsPerSample == 8)
			format = AL_FORMAT_MONO8;
		else if (wav.channels == 1 && wav.bitsPerSample == 16)
			format = AL_FORMAT_MONO16;
		else if (wav.channels == 2 && wav.bitsPerSample == 8)
			format = AL_FORMAT_STEREO8;
		else if (wav.channels == 2 && wav.bitsPerSample == 16)
			format = AL_FORMAT_STEREO16;
		else
		{
			std::cerr << "[AudioClip] Unsupported channel/bit-depth combination in: " << path << std::endl;
			return false;
		}

		alGenBuffers(1, &m_BufferID);
		alBufferData(m_BufferID, format, wav.pcmData.data(), static_cast<ALsizei>(wav.pcmData.size()), wav.sampleRate);

		int bytesPerSample = wav.bitsPerSample / 8;
		int blockAlign = bytesPerSample * wav.channels;
		if (blockAlign > 0 && wav.sampleRate > 0)
		{
			size_t totalSamples = wav.pcmData.size() / blockAlign;
			m_Duration = static_cast<float>(totalSamples) / static_cast<float>(wav.sampleRate);
		}

		return true;
	}
}
