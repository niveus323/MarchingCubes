#pragma once
#include "Core/Geometry/MarchingCubes/ITerrainBackend.h"
#include "Core/DataStructures/Data.h"
#include <deque>

class DescriptorAllocator;
class UploadContext;
class SDFVolume3D;
class GPUBrushCS;
class GPUMarchingCubesCS;

struct OutTriangle
{
	Vertex C;
	Vertex B;
	Vertex A;
	uint32_t chunkIdx;
};

class GPUTerrainBackend : public ITerrainBackend
{
private:
	struct ChunkSlot
	{
		uint32_t index = 0;                   // 슬롯 번호
		ComPtr<ID3D12Resource> volumeTexture; // 18^3 크기의 3D 텍스처
		uint32_t srvIndex = 0;                // SRV 힙 인덱스 (쉐이더 바인딩용)

		uint64_t completionFence = 0;         // 이 슬롯의 작업이 끝나는 Fence 값
		bool isFree = true;                   // CPU 사용 가능 여부
	};
	struct PendingRequest
	{
		BuildRequest request;
	};
	struct InFlightJob
	{
		uint32_t slotIndex;      // 사용한 슬롯
		BuildRequest request;    // 요청 원본
		// 나중에 Readback 시 오프셋 정보 등도 여기에 추가됨
	};
public:
	explicit GPUTerrainBackend(ID3D12Device* device, DescriptorAllocator* descriptorAllocator);
	~GPUTerrainBackend();

	void Initialize(uint32_t poolSize = 64, uint32_t maxChunkSize = 32);
	void PushRequest(BuildRequest&& request) override;
	bool TryFetch(std::vector<BuildResult>& OutResults) override;
	void ExecuteCompute(uint32_t frameIndex, UploadContext* uploadContext, DescriptorAllocator* descriptorAllocator);
	
	bool HasRequests() const override 
	{
		if (!m_waitingQueue.empty() || !m_inFlightJobs.empty()) return true;
		return false;
	}
private:
	// TexturePool
	void InitializeSlots(ID3D12Device* device, int count, int slotSize);
	void CreateSlotResource(int index);
	void ExtractChunkData(const SdfField* field, const ChunkKey& key, const GridDesc& desc, std::vector<float>& outBuffer);
	
private:
	static constexpr uint32_t MAX_TRIANGLES_PER_CHUNK = 22000;
	static constexpr uint32_t MAX_TOTAL_TRIANGLES = MAX_TRIANGLES_PER_CHUNK * 64;

	ID3D12Device* m_device = nullptr;
	DescriptorAllocator* m_descriptorAllocator = nullptr;

	ComPtr<ID3D12CommandQueue> m_commandQueue;
	ComPtr<ID3D12CommandAllocator> m_commandAllocator;
	ComPtr<ID3D12GraphicsCommandList> m_commandList;
	ComPtr<ID3D12Fence> m_fence;
	HANDLE m_fenceEvent;
	uint64_t m_fenceValues = 0;
	uint32_t m_triTableSlot = UINT32_MAX;

	// Resources
	std::vector<ChunkSlot> m_chunkSlots;          // 슬롯 풀
	std::deque<PendingRequest> m_waitingQueue;    // 대기열
	std::deque<InFlightJob> m_inFlightJobs;       // 실행 중인 작업

	std::vector<float> m_tempExtractBuffer;       // 데이터 추출용 재사용 버퍼
	uint32_t m_maxChunkSize = 32;

	ComPtr<ID3D12Resource> m_outBuffer;
	ComPtr<ID3D12Resource> m_outCounter;
	uint32_t m_outBufferUavIndex = 0;   

	ComPtr<ID3D12Resource> m_readbackBuffer;
	uint64_t m_lastReadbackFence = 0;

	//BufferHandle m_gridCB;

	std::unique_ptr<GPUMarchingCubesCS> m_mc;
};

