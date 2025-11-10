#include "pch.h"
#include "Timer.h"
#include <string>
#include <atomic>

static inline double AvgMs(const std::deque<double>& q)
{
	if (q.empty()) return 0.0;
	long double s = 0.0;
	for (double v : q) s += v;
	return static_cast<double>(s / q.size());
}

void Timer::Start()
{
	m_prev = std::chrono::steady_clock::now();
	m_lastCpuMs = 0.0;
	m_lastGpuMs = 0.0;
	m_cpuMsQ.clear();
	m_gpuMsQ.clear();
}

float Timer::Tick()
{
	auto now = std::chrono::steady_clock::now();
	std::chrono::duration<float> elapsed = now - m_prev;
	m_prev = now;
	m_lastCpuMs = static_cast<double>(elapsed.count()) * 1000.0;
	if (m_maxSamples == 0) return elapsed.count();
	m_cpuMsQ.push_back(m_lastCpuMs);
	m_gpuMsQ.push_back(m_lastGpuMs);
	if (m_cpuMsQ.size() > m_maxSamples) m_cpuMsQ.pop_back();
	return elapsed.count();
}

double Timer::GetCpuFrameMsAvg() const
{
	return AvgMs(m_cpuMsQ);
}

float Timer::GetCpuFPSAvg() const
{
	const double ms = GetCpuFrameMsAvg();
	return ms > 0.0 ? static_cast<double>(1000.0 / ms) : 0.0f;
}

void Timer::PushGpuFrameMs(double ms)
{
	m_lastGpuMs = ms;
	if (m_maxSamples == 0) return;
	m_gpuMsQ.push_front(ms);
	if (m_gpuMsQ.size() > m_maxSamples) m_gpuMsQ.pop_back();
}

double Timer::GetGpuFrameMsAvg() const
{
	return AvgMs(m_gpuMsQ);
}

float Timer::GetGpuFPSAvg() const
{
	const double ms = GetGpuFrameMsAvg();
	return ms > 0.0 ? static_cast<float>(1000.0 / ms) : 0.0f;
}

void Timer::SetFpsSampleCount(size_t n)
{
	m_maxSamples = n;
	ResetFps();
}

void Timer::ResetFps()
{
	m_cpuMsQ.clear(); 
	m_gpuMsQ.clear();
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
	return ms;
}
