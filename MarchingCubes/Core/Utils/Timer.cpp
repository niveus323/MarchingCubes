#include "pch.h"
#include "Timer.h"
#include <string>
#include <atomic>
#include "Core/Utils/Log.h"

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

void Timer::BeginKey(std::string_view key)
{
	std::lock_guard<std::mutex> lock(m_storedKey.mtx);
	m_storedKey.stackMap[std::string(key)].push_back(clock::now());
}

double Timer::EndKey(std::string_view key)
{
	Time_point t0{};
	{
		std::lock_guard<std::mutex> lock(m_storedKey.mtx);
		auto& vec = m_storedKey.stackMap[std::string(key)];
		if (vec.empty())
		{
			Log::Print("Timer", "Timer::EndKey : key '%.*s' has no active begin", (int)key.size(), key.data());
			return -1.0f;
		}
		t0 = vec.back();
		vec.pop_back();
	}

	const auto t1 = clock::now();

	const double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
	Log::Print("Timer", "'%.*s' : %.3f ms", (int)key.size(), key.data(), ms);

	return ms;
}
