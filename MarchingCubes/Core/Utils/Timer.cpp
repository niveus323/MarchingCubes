#include "pch.h"
#include "Timer.h"

void Timer::Start()
{
	m_prev = std::chrono::steady_clock::now();
}

float Timer::Tick()
{
	auto now = std::chrono::steady_clock::now();
	std::chrono::duration<float> elapsed = now - m_prev;
	m_prev = now;
	return elapsed.count();
}
