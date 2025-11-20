#include "pch.h"
#include "Profiler.h"

Profiler::Profiler()
{
	m_readIndex.store(0);
	m_snapshots[0] = ProfilerSnapshot{};
	m_snapshots[1] = ProfilerSnapshot{};
}

Profiler::~Profiler() = default;

bool Profiler::RegisterMetric(const std::string& name, Metric meta)
{
	std::lock_guard<std::mutex> lock(m_writeMutex);
	if (m_metrics.find(name) != m_metrics.end()) return false;
	m_metrics[name] = std::move(meta);
	return true;
}

bool Profiler::UnregisterMetric(const std::string& name)
{
	std::lock_guard<std::mutex> lg(m_writeMutex);
	m_metrics.erase(name);
	return true;
}

void Profiler::SetMetric(const std::string& name, const MetricValue& v)
{
	std::lock_guard<std::mutex> lock(m_writeMutex);
	m_metrics[name].value = v;
}

void Profiler::PushHistogram(const std::string& name, double sample)
{
	std::lock_guard<std::mutex> lock(m_writeMutex);
	auto iter = m_metrics.find(name);
	if (iter != m_metrics.end() && iter->second.type == MetricType::Histogram)
	{
		Histogram& h = std::get<Histogram>(m_metrics[name].value);
		h["last"] = sample;
	}
	else 
	{
		m_metrics[name].value = sample;
	}
}

void Profiler::SetBufferPools(const std::vector<BufferPoolInfo>& pools)
{
	std::lock_guard<std::mutex> lock(m_writeMutex);
	m_currentPools = pools;
}

void Profiler::UpdateFrame(uint64_t frameTimestamp)
{
	int w = GetWritableindex();
	{
		std::lock_guard<std::mutex> lock(m_writeMutex);
		ProfilerSnapshot& snap = m_snapshots[w];
		snap.timestamp = frameTimestamp;
		snap.metrics = m_metrics;
		
		snap.pools = m_currentPools;
	}

	m_readIndex.store(w, std::memory_order_release);
}

const ProfilerSnapshot& Profiler::GetReadSnapshot() const
{
	int r = m_readIndex.load(std::memory_order_acquire);
	return m_snapshots[r];
}
