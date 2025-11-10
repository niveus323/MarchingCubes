#pragma once
#include <chrono>
#include <mutex>
#include <unordered_map>
#include <vector>
#include <deque>

class Timer
{
	using clock = std::chrono::steady_clock;
	using Time_point = clock::time_point;
public:
	void Start();
	float Tick();
	static uint64_t GetTimeMs() { return duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count(); }

	double GetCpuFrameMs() const { return m_lastCpuMs; }
	float GetCpuFPS() const { return m_lastCpuMs > 0 ? float(1000.0 / m_lastCpuMs) : 0.0f; }
	double GetCpuFrameMsAvg() const;
	float GetCpuFPSAvg() const;

	void PushGpuFrameMs(double ms);
	double GetGpuFrameMs() const { return m_lastGpuMs; }
	float GetGpuFPS() const { return m_lastGpuMs > 0 ? float(1000.0 / m_lastGpuMs) : 0.0f; }
	double GetGpuFrameMsAvg() const;
	float GetGpuFPSAvg() const;

	void SetFpsSampleCount(size_t n);
	void ResetFps();

	template <typename F, typename... Args>
	static auto MeasureCall(F&& f, Args&&... args)
	{
		auto t0 = clock::now();

		if constexpr (std::is_void_v<std::invoke_result_t<F, Args...>>)
		{
			std::invoke(std::forward<F>(f), std::forward<Args>(args)...);
			auto t1 = clock::now();
			return std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0);
		}
		else
		{
			auto ret = std::invoke(std::forward<F>(f), std::forward<Args>(args)...);
			auto t1 = clock::now();
			return std::pair{ std::move(ret), std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0) };
		}
	}
	
	template <typename F, typename... Args>
	static double MeasureMs(F&& f, Args&&... args)
	{
		if constexpr (std::is_void_v<std::invoke_result_t<F, Args...>>) {
			auto ns = MeasureCall(std::forward<F>(f), std::forward<Args>(args)...);
			return ns.count() / 1'000'000.0;
		}
		else {
			auto [ret, ns] = MeasureCall(std::forward<F>(f), std::forward<Args>(args)...);
			(void)ret;
			return ns.count() / 1'000'000.0;
		}
	}

	// 멤버 함수 포인터(비-const/const) 호출 편의 오버로드
    template <class C, class R, class... MArgs, class Obj, class... Args>
    static double MeasureMs(R(C::* mf)(MArgs...), Obj&& obj, Args&&... args) {
        return MeasureMs([&] {
            std::invoke(mf, std::forward<Obj>(obj), std::forward<Args>(args)...);
        });
    }
    template <class C, class R, class... MArgs, class Obj, class... Args>
    static double MeasureMs(R(C::* mf)(MArgs...) const, Obj&& obj, Args&&... args) {
        return MeasureMs([&] {
            std::invoke(mf, std::forward<Obj>(obj), std::forward<Args>(args)...);
        });
    }

#ifdef _WIN32
	template <typename F, typename... Args>
	static double MeasureMsQPC(F&& f, Args&&... args)
	{
		LARGE_INTEGER freq{}, t0{}, t1{};
		QueryPerformanceFrequency(&freq);
		QueryPerformanceCounter(&t0);

		std::invoke(std::forward<F>(f), std::forward<Args>(args)...);

		QueryPerformanceCounter(&t1);
		return (t1.QuadPart - t0.QuadPart) * 1000.0 / static_cast<double>(freq.QuadPart);
	}
#endif // _WIN32

	static void BeginKey(std::string_view key);
	static double EndKey(std::string_view key);

private:
	Time_point m_prev;

	// FPS & FrameTime
	size_t m_maxSamples = 120;
	std::deque<double> m_cpuMsQ;
	std::deque<double> m_gpuMsQ;
	double m_lastCpuMs = 0.0;
	double m_lastGpuMs = 0.0;

	struct TimerKey
	{
		std::mutex mtx;
		std::unordered_map<std::string, std::vector<std::chrono::steady_clock::time_point>> stackMap;
	};
	inline static TimerKey m_storedKey{};
};