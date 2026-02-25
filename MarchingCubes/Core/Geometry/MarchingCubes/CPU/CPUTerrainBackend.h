#pragma once
#include "Core/Geometry/MarchingCubes/ITerrainBackend.h"
#include <mutex>
#include <future>

class CPUTerrainBackend :   public ITerrainBackend
{
public:
	CPUTerrainBackend(ID3D12Device* device);
	~CPUTerrainBackend();

	// ITerrainBackend을(를) 통해 상속됨
	void PushRequest(BuildRequest&& request) override;
	bool TryFetch(std::vector<BuildResult>& OutResults) override;
	bool HasRequests() const override;
private:
	void ProcessMarchingCubes(BuildRequest& request, BuildResult& result);
	void CleanupFinishedTasks();
private:
	mutable std::mutex m_resultMutex;
	std::vector<BuildResult> m_results;
	std::vector<std::future<void>> m_runningTasks;
};