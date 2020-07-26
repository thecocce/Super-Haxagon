#include "Factories/Pattern.hpp"

#include "Core/Twist.hpp"
#include "Driver/Platform.hpp"

namespace SuperHaxagon {
	const char* PatternFactory::PATTERN_HEADER = "PTN1.1";
	const char* PatternFactory::PATTERN_FOOTER = "ENDPTN";

	Pattern::Pattern(std::vector<Wall>& walls, const int sides) : _walls(std::move(walls)), _sides(sides) {}

	double Pattern::getFurthestWallDistance() const {
		double maxDistance = 0;
		for(const auto& wall : _walls) {
			const auto distance = wall.getDistance() + wall.getHeight();
			if(distance > maxDistance) maxDistance = distance;
		}

		return maxDistance;
	}

	void Pattern::advance(const double speed) {
		for(auto& wall : _walls) {
			wall.advance(speed);
		}
	}

	PatternFactory::PatternFactory(std::ifstream& file, Platform& platform) {
		_name = readString(file, platform, "pattern name");

		if (!readCompare(file, PATTERN_HEADER)) {
			platform.message(Dbg::WARN, "pattern", _name + " pattern header invalid!");
			return;
		}

		// This might be able to be increased later
		_sides = read32(file, 0, 256, platform, _name + " pattern sides");
		if(_sides < MIN_PATTERN_SIDES) _sides = MIN_PATTERN_SIDES;

		const int numWalls = read32(file, 1, 1000, platform, _name + " pattern walls");
		for (auto i = 0; i < numWalls; i++) _walls.emplace_back(file, _sides);

		if (!readCompare(file, PATTERN_FOOTER)) {
			platform.message(Dbg::WARN, "pattern", _name + " pattern footer invalid!");
			return;
		}

		_loaded = true;
	}

	PatternFactory::~PatternFactory() = default;

	Pattern PatternFactory::instantiate(Twist& rng, const double distance) const {
		const auto offset = rng.rand(_sides - 1);
		std::vector<Wall> active;
		for(const auto& wall : _walls) {
			active.emplace_back(wall.instantiate(distance, offset, _sides));
		}

		return {active, _sides};
	}
}
