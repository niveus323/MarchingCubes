#pragma once
#include <chrono>

class Timer
{
public:
	void Start();
	float Tick();

private:
	std::chrono::steady_clock::time_point m_prev;
};
