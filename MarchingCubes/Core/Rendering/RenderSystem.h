#pragma once
#include "PSO/PSOList.h"
#include "BundleRecorder.h"
#include "Core/Geometry/Mesh/Mesh.h"
#include "Core/Rendering/UploadContext.h"
#include "Memory/GpuAllocator.h"
#include "Memory/StaticBufferRegistry.h"

struct PSOBucket
{
	StaticRenderItem staticItem; // 정적 번들 (PSO 단위이므로 컨테이너 사용 x)
	std::vector<DynamicRenderItem> dynamicItems; // 동적 오브젝트
};

class RenderSystem
{
public:
	RenderSystem(ID3D12Device* device, ID3D12RootSignature* rootSignature, const D3D12_INPUT_LAYOUT_DESC& inputLayout, const std::vector<std::wstring>& psoFiles);
	~RenderSystem();

	GpuAllocator* GetGpuAllocator() { return m_gpuAllocator.get(); }
	StaticBufferRegistry* GetStaticBufferRegistry() { return m_staticBufferRegistry.get(); }
	PSOList* GetPSOList() { return m_psoList.get(); }
	BundleRecorder* GetBundleRecorder() { return m_bundleRecorder.get(); }
	const std::vector<PSOBucket>& GetBuckets() { return m_buckets; }
	bool GetPsoEnabled(int psoIndex) { return m_passEnabled[psoIndex]; }
	UploadContext* GetUploadContext() { return m_uploadContext.get(); }
	inline int GetPSOIndex(const std::string& psoName) { return m_psoList->IndexOf(psoName); }
	inline void SetPsoEnabled(const std::string& psoName, bool bEnabled) { m_passEnabled[GetPSOIndex(psoName)] = bEnabled; }

	void PrepareRender(ID3D12GraphicsCommandList* cmd);
	void RenderFrame(ID3D12GraphicsCommandList* cmd, const CameraConstants& cameraData, const LightBlobView& lightData);
	PendingDeleteItem CleanUp(UINT64 fenceValue);
	bool IsDynamicRegistered(IDrawable* drawable, const std::string& psoName);
	bool RegisterStatic(IDrawable* drawable, const std::string& psoName);
	bool RegisterDynamic(IDrawable* drawable, const std::string& psoName);
	bool UnRegisterStatic(IDrawable* drawable, const std::string& psoName);
	bool UnRegisterDynamic(IDrawable* drawable, const std::string& psoName);
	bool UpdateDynamic(IDrawable* drawable, const GeometryData& data);
	void CallWhenFenceSignaled(UINT64 fenceValue); // TODO : 이름 변경

#ifdef _DEBUG
	void SetWireViewEnabled(bool enable) { m_wireViewEnabled = enable; }
#endif // _DEBUG


private:
	void UploadCameraConstants(const CameraConstants& cameraData);
	void UploadLightConstants(const LightBlobView& lightBlobView);
	void BindCommonResources(ID3D12GraphicsCommandList* cmd);

private:
	std::unique_ptr<PSOList> m_psoList;
	std::unique_ptr<BundleRecorder> m_bundleRecorder;
	std::unique_ptr<GpuAllocator> m_gpuAllocator;
	std::unique_ptr<UploadContext> m_uploadContext;
	std::unique_ptr<StaticBufferRegistry> m_staticBufferRegistry;

	std::vector<PSOBucket> m_buckets;
	std::vector<bool> m_passEnabled;

	// camera
	ComPtr<ID3D12Resource> m_cameraCB;
	uint8_t* m_cameraCBMapped = nullptr;

	// lights
	ComPtr<ID3D12Resource> m_lightCB;
	uint8_t* m_lightCBMapped = nullptr;
	UINT m_lightCBSize = 0;

	std::vector<ComPtr<ID3D12Resource>> m_toDeletesContainer;

	// app 클래스에서 Fence가 업데이트 되었을 때 마다 Fence 값을 갱신해준다.
	UINT64 m_completedFenceValue = 0;

#ifdef _DEBUG
	bool m_wireViewEnabled = false;
#endif // _DEBUG

};

