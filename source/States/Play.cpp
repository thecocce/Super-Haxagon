#include "States/Play.hpp"

#include "Core/Game.hpp"
#include "Core/Metadata.hpp"
#include "Driver/Font.hpp"
#include "Driver/Platform.hpp"
#include "Driver/Player.hpp"
#include "Factories/Level.hpp"
#include "States/Over.hpp"
#include "States/Quit.hpp"

#include <array>
#include <cmath>

namespace SuperHaxagon {

	Play::Play(Game& game, LevelFactory& factory, const int levelIndex) :
		_game(game),
		_platform(game.getPlatform()),
		_factory(factory),
		_level(factory.instantiate(game.getTwister(), SCALE_BASE_DISTANCE)),
		_levelIndex(levelIndex)
	{}

	Play::~Play() = default;

	void Play::enter() {
		auto* bgm = _platform.getBGM();
		if (bgm) bgm->play();
		_platform.playSFX(_game.getSFXBegin());
		_game.setShadowAuto(true);
	}

	void Play::exit() {
		auto* bgm = _platform.getBGM();
		if (bgm) bgm->pause();
	}

	std::unique_ptr<State> Play::update(const double dilation) {
		const auto maxRenderDistance = SCALE_BASE_DISTANCE * (_game.getScreenDimMax() / 400);

		// Render the level with a skewed 3D look
		auto skewFrameMax = _level->getLevelFactory().getSpeedPulse() * 2.5;
		skewFrameMax = skewFrameMax < SKEW_MIN_FRAMES ? SKEW_MIN_FRAMES : skewFrameMax;
		_skewFrame += dilation * _skewDirection * (_level->getLevelFactory().getSpeedRotation() > 0 ? 1 : 0);
		_game.setSkew((-cos(_skewFrame / skewFrameMax * PI) + 1.0) / 2.0 * SKEW_MAX);

		// Get effect data
		auto& metadata = _game.getBGMMetadata();
		const auto* bgm = _platform.getBGM();
		const auto time = bgm ? bgm->getTime() : 0.0;

		// Apply effects. More can be added here if needed.
		if (metadata.getMetadata(time, "S")) _level->spin();
		if (metadata.getMetadata(time, "I")) _level->invertBG();
		if (metadata.getMetadata(time, "B")) _level->pulse();

		// Update level
		_level->update(_game.getTwister(), SCALE_HEX_LENGTH, maxRenderDistance, dilation);

		// Button presses
		const auto pressed = _platform.getPressed();

		// Check collision
		const auto cursorDistance = SCALE_HEX_LENGTH + SCALE_HUMAN_PADDING + SCALE_HUMAN_HEIGHT;
		const auto hit = _level->collision(cursorDistance, dilation);

		// Keys
		if(pressed.back || hit == Movement::DEAD) {
			return std::make_unique<Over>(_game, _factory, std::move(_level), _score, _levelIndex);
		}

		if (pressed.quit) {
			return std::make_unique<Quit>(_game);
		}

		if (pressed.left && hit != Movement::CANNOT_MOVE_LEFT) {
			_level->left(dilation);
		} else if (pressed.right && hit != Movement::CANNOT_MOVE_RIGHT) {
			_level->right(dilation);
		}

		// Make sure the cursor doesn't extend too far
		_level->clamp();

		// Update score
		const auto* lastScoreText = getScoreText(static_cast<int>(_score), false);
		_score += dilation;

		if(lastScoreText != getScoreText(static_cast<int>(_score), false)) {
			_level->increaseMultiplier();
			_platform.playSFX(_game.getSFXLevelUp());
		}

		return nullptr;
	}

	void Play::drawTop(const double scale) {
		_level->draw(_game, scale, 0);
	}

	void Play::drawBot(const double scale) {
		auto& small = _game.getFontSmall();

		// Makes it so the score text doesn't freak out
		// if getWidth returns slightly different values
		// each frame
		if (_scalePrev != scale) {
			_scalePrev = scale;
			small.setScale(scale);
			_scoreWidth = small.getWidth("TIME: " + getTime(0));
		}

		const auto width = _platform.getScreenDim().x;
		const auto pad = 3 * scale;

		small.setScale(scale);

		// Draw the top left POINT/LINE thing
		// Note, 400 is kind of arbitrary. Perhaps it's needed to update this later.
		const auto* levelUp = getScoreText(static_cast<int>(_score), _platform.getScreenDim().x <= 400);
		const Point levelUpPosText = {pad, pad};
		const Point levelUpBkgPos = {0, 0};
		const Point levelUpBkgSize = {
			levelUpPosText.x + small.getWidth(levelUp) + pad,
			levelUpPosText.y + small.getHeight() + pad
		};

		const std::array<Point, 3> levelUpBkgTri = {
			Point{levelUpBkgSize.x, levelUpBkgSize.y},
			Point{levelUpBkgSize.x, 0},
			Point{levelUpBkgSize.x + levelUpBkgSize.y/2, 0}
		};

		_platform.drawRect(COLOR_TRANSPARENT, levelUpBkgPos, levelUpBkgSize);
		_platform.drawTriangle(COLOR_TRANSPARENT, levelUpBkgTri);
		small.draw(COLOR_WHITE, levelUpPosText, Alignment::LEFT, levelUp);

		// Draw the current score
		const auto textScore = "TIME: " + getTime(_score);
		const Point scorePosText = { width - pad - _scoreWidth, pad};
		const Point scoreBkgPos = {scorePosText.x - pad, 0};
		Point scoreBkgSize = {
			width - scoreBkgPos.x,
			scorePosText.y + small.getHeight() + pad
		};

		// Before we draw the score, compute the best time graph.
		auto drawBar = false;
		auto drawHigh = false;
		const auto originalY = scoreBkgSize.y;
		const auto heightBar = 2 * scale;
		if (_factory.getHighScore() > 0) {
			if (_score < _factory.getHighScore()) {
				scoreBkgSize.y += heightBar + pad;
				drawBar = true;
			} else {
				scoreBkgSize.y += small.getHeight() + pad;
				drawHigh = true;
			}
		}

		const std::array<Point, 3> scoreBkgTri = {
			Point{scoreBkgPos.x, scoreBkgSize.y},
			Point{scoreBkgPos.x, 0},
			Point{scoreBkgPos.x - scoreBkgSize.y/2, 0}
		};

		_platform.drawTriangle(COLOR_TRANSPARENT, scoreBkgTri);
		_platform.drawRect(COLOR_TRANSPARENT, scoreBkgPos, scoreBkgSize);
		small.draw(COLOR_WHITE, scorePosText, Alignment::LEFT, textScore);

		if (drawBar) {
			const Point barPos = {scorePosText.x, originalY};
			const Point barWidth = {_scoreWidth, heightBar};
			const Point barWidthScore = {_scoreWidth * (_score / _factory.getHighScore()), heightBar};
			_platform.drawRect(COLOR_BLACK, barPos, barWidth);
			_platform.drawRect(COLOR_WHITE, barPos, barWidthScore);
		}

		if (drawHigh) {
			auto textColor = COLOR_WHITE;
			const Point posBest = {width - pad, originalY};
			if (_score - _factory.getHighScore() <= PULSE_TIMES * PULSE_TIME) {
				const auto percent = getPulse(_score, PULSE_TIME, _factory.getHighScore());
				textColor = interpolateColor(PULSE_LOW, PULSE_HIGH, percent);
			}

			small.draw(textColor, posBest, Alignment::RIGHT,  "NEW RECORD!");
		}
	}
}
