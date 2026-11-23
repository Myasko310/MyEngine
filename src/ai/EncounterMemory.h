#pragma once

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <string>
#include <vector>
#include <glm/glm.hpp>

namespace MyEngine
{
	struct EncounterMemory
	{
		struct Place
		{
			glm::vec3 position{}, objectPosition{};
			unsigned visits = 0;
			std::string object;
			unsigned missingChecks = 0;
		};
		static constexpr std::size_t capacity = 512;
		std::vector<Place> places;
		std::vector<glm::vec3> stalls;
		bool trackingMovement = false, crossing = false;
		glm::vec3 previousPosition{}, crossingPoint{}, entrySide{};
		void TrackCrossing(glm::vec3 p)
		{
			if (!Valid(p)) { trackingMovement = crossing = false; return; }
			const auto previous = previousPosition;
			previousPosition = p;
			if (!trackingMovement || glm::distance(previous, p) > 1.0f)
			{
				trackingMovement = true; crossing = false; return;
			}
			auto horizontal = [](glm::vec3 v) { return glm::vec3(v.x, 0, v.z); };
			if (!crossing)
			{
				for (const auto& stall : stalls)
				{
					const auto side = horizontal(previous - stall);
					if (std::abs(p.y - stall.y) > 1.0f || glm::length(side) < 0.55f || glm::length(horizontal(p - stall)) >= 0.55f) continue;
					entrySide = glm::normalize(side); crossingPoint = stall; crossing = true; break;
				}
			}
			else if (std::abs(p.y - crossingPoint.y) > 1.0f) crossing = false;
			else if (glm::length(horizontal(p - crossingPoint)) >= 0.65f)
			{
				if (glm::dot(horizontal(p - crossingPoint), entrySide) < -0.4f)
					stalls.erase(std::remove(stalls.begin(), stalls.end(), crossingPoint), stalls.end());
				crossing = false;
			}
		}
		void NoteStall(glm::vec3 p)
		{
			if (!Valid(p)) return;
			auto old = std::find_if(stalls.begin(), stalls.end(), [&](glm::vec3 q) { return glm::distance(p,q) < 0.5f; });
			if (old != stalls.end()) stalls.erase(old);
			if (stalls.size() == 32) stalls.erase(stalls.begin());
			stalls.push_back(p);
		}
		std::filesystem::path file;
		float sampleDelay = 0.0f;
		bool loaded = false, hasLast = false;
		glm::vec3 lastCell{};

		static glm::vec3 Cell(glm::vec3 p) { return glm::floor(p / 3.0f); }
		static bool Valid(glm::vec3 p)
		{
			return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z);
		}

		void Observe(glm::vec3 p, const std::string& object, glm::vec3 objectPosition, bool preserveLandmark = false)
		{
			if (!Valid(p) || !Valid(objectPosition)) return;
			const auto cell = Cell(p);
			auto it = std::find_if(places.begin(), places.end(), [&](const Place& place) { return Cell(place.position) == cell; });
			if (it == places.end())
			{
				if (places.size() == capacity) places.erase(places.begin());
				places.push_back({p});
				it = places.end() - 1;
			}
			if (!hasLast || cell != lastCell || !it->visits)
				it->visits = (std::min)(it->visits + 1, 1000000u);
			lastCell = cell; hasLast = true;
			if (preserveLandmark && !it->object.empty() && it->object != object) return;
			it->object = object.substr(0, 160);
			it->objectPosition = objectPosition;
			it->missingChecks = 0;
		}

		unsigned Familiarity(glm::vec3 p) const
		{
			unsigned score = 0;
			for (const auto& place : places)
			{
				const auto offset = place.position - p;
				if (glm::dot(offset, offset) <= 2.25f)
					score = (std::max)(score, place.visits + !place.object.empty());
			}
			return score;
		}

		bool Load(const std::filesystem::path& path)
		{
			*this = EncounterMemory{};
			file = path;
			std::error_code ec;
			if (!std::filesystem::exists(path, ec)) return !ec;
			if (std::filesystem::file_size(path, ec) > 512 * 1024 || ec) return false;
			std::ifstream in(path);
			std::string version;
			std::size_t count = 0;
			if (!(in >> version >> count) || (version != "EncounterMemory1" && version != "EncounterMemory2") || count > capacity) return false;
			std::vector<Place> data(count);
			for (auto& p : data)
				if (!(in >> p.position.x >> p.position.y >> p.position.z >> p.visits >> std::quoted(p.object) >> p.objectPosition.x >> p.objectPosition.y >> p.objectPosition.z) || !Valid(p.position) || !Valid(p.objectPosition) || !p.visits || p.visits > 1000000 || p.object.size() > 160) return false;
			if (version == "EncounterMemory2")
			{
				std::size_t n;
				if (!(in >> n) || n > 32) return false;
				std::vector<glm::vec3> hints(n);
				for (auto& p : hints) if (!(in >> p.x >> p.y >> p.z) || !Valid(p)) return false;
				stalls.swap(hints);
			}
			places.swap(data);
			loaded = !places.empty();
			return true;
		}

		bool Save() const
		{
			if (file.empty()) return true;
			std::error_code ec;
			std::filesystem::create_directories(file.parent_path(), ec);
			if (ec) return false;
			auto tmp = file; tmp += ".tmp";
			std::ofstream out(tmp, std::ios::trunc);
			out << "EncounterMemory2 " << places.size() << '\n' << std::setprecision(std::numeric_limits<float>::max_digits10);
			for (const auto& p : places)
				out << p.position.x << ' ' << p.position.y << ' ' << p.position.z << ' ' << p.visits << ' ' << std::quoted(p.object) << ' ' << p.objectPosition.x << ' ' << p.objectPosition.y << ' ' << p.objectPosition.z << '\n';
			out << stalls.size() << '\n';
			for (auto p : stalls) out << p.x << ' ' << p.y << ' ' << p.z << '\n';
			out.close();
			if (!out) return false;
			std::filesystem::rename(tmp, file, ec);
			return !ec;
		}
	};
}
