#pragma once

#include <chrono>
#include <deque>

struct FpsCounter {
public:
	FpsCounter() : _prevTick(std::chrono::high_resolution_clock::now()) {
	}

	[[nodiscard]] float getFrameTime() const {
		return _frameTimes.back();
	}
	[[nodiscard]] float getFpsRunningAverage() const {
		return 1.0f / getFrameTime();
	}
	[[nodiscard]] float getFpsAverageWindow() const {
		return _frameTimes.size() - std::max(_totalFrameTime - timeWindow, 0.0f) / _frameTimes.front();
	}

	void tick() {
		auto now = std::chrono::high_resolution_clock::now();
		float frameTime = std::chrono::duration<float>(now - _prevTick).count();

		_frameTimeRunningAverage = alpha * frameTime + (1.0f - alpha) * _frameTimeRunningAverage;

		_frameTimes.emplace_back(frameTime);
		_totalFrameTime += frameTime;
		while (_totalFrameTime - _frameTimes.front() > timeWindow) {
			_totalFrameTime -= _frameTimes.front();
			_frameTimes.pop_front();
		}

		_prevTick = now;
	}

	float alpha = 0.05f;
	float timeWindow = 1.0f;
private:
	std::chrono::high_resolution_clock::time_point _prevTick;

	float _frameTimeRunningAverage = 0.0f;

	std::deque<float> _frameTimes;
	float _totalFrameTime = 0.0f;
};
