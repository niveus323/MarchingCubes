#pragma once
#include <map>
#include <variant>
#include <unordered_map>
#include <mutex>
#include "Core/Rendering/Memory/CommonMemory.h"

/* -------- Profiler ---------
* 프로파일링을 위해 필요한 데이터를 저장해두는 클래스
* SnapShot을 가곧하여 저장해두고 UI 렌더링, 로그 출력 등은 SnapShot을 처리하는 측에서 책임을 갖는다.
* ----------------------------
*/

enum class MetricType
{
	Value = 0,
	Gauge,
	Counter,
	Histogram,
	Text
};

using Histogram = std::map<std::string, double>;
using MetricValue = std::variant<int64_t, double, std::string, std::vector<float>, Histogram>;

struct Metric
{
	MetricType type = MetricType::Gauge;
	MetricValue value;
	std::string unit;
	std::string description;
	int updateHintHz = 60;

};

struct ProfilerSnapshot
{
	uint64_t timestamp = 0;
	std::unordered_map<std::string, Metric> metrics;
	// Debugging Fields
	std::vector<MemoryInfo> pools;
	std::vector<DedicatedBufferInfo> dedicatedBuffers;
};


class Profiler
{
public:
	Profiler();
	~Profiler();
	
	// Metric registration 
	bool RegisterMetric(const std::string& name, Metric meta);
	bool UnregisterMetric(const std::string& name);

	// Producers update metrics
	void SetMetric(const std::string& name, const MetricValue& v);   // set latest value
	void PushHistogram(const std::string& name, double sample);        // append for histogram/series

	// For larger/structured data (owner pools), set via this API:
	void SetBufferPools(const std::vector<MemoryInfo>& pools);
	void SetDedicatedBuffers(const std::vector<DedicatedBufferInfo>& buffers);

	// Called once per frame on update thread: captures current state into a write snapshot and swaps
	void UpdateFrame(uint64_t frameTimestamp);

	// UI thread (or same thread) can call to read snapshot without taking locks
	// Returned reference is valid until next UpdateFrame call.
	const ProfilerSnapshot& GetReadSnapshot() const;

private:
	int GetWritableindex() const { return 1 - m_readIndex.load(std::memory_order_acquire); }

private:
	std::mutex m_writeMutex;
	std::unordered_map<std::string, Metric> m_metrics;

	std::vector<MemoryInfo> m_currentPools;
	std::vector<DedicatedBufferInfo> m_dedicatedBuffers;

	// double-buffered snapshots (swap via atomic index)
	ProfilerSnapshot m_snapshots[2];
	std::atomic<int> m_readIndex{ 0 };  // index that UI reads
};

