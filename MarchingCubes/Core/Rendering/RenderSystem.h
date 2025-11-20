#pragma once
#include "PSO/PSOList.h"
#include "BundleRecorder.h"
#include "Core/Geometry/Mesh/Mesh.h"
#include "Core/Rendering/UploadContext.h"
#include "Memory/GpuAllocator.h"
#include "Memory/StaticBufferRegistry.h"

struct PSOBucket
{
	std::vector<IDrawable*> staticItems; // 정적 오브젝트
	std::vector<IDrawable*> dynamicItems; // 동적 오브젝트
	//std::unordered_map<BundleKey, BundleGroup> bundles;
};

struct RenderSystemInitInfo
{
	ID3D12Device* device = nullptr;
	ID3D12RootSignature* rootSignature = nullptr;
	std::vector<D3D12_INPUT_ELEMENT_DESC> inputElements;
	std::vector<std::wstring> psoFiles;
};

class RenderSystem
{
public:
	explicit RenderSystem(RenderSystemInitInfo init_info);
	RenderSystem(ID3D12Device* device, ID3D12RootSignature* rootSignature, const D3D12_INPUT_LAYOUT_DESC& inputLayout, const std::vector<std::wstring>& psoFiles);
	~RenderSystem();

	PSOList* GetPSOList() { return m_psoList.get(); }
	BundleRecorder* GetBundleRecorder() { return m_bundleRecorder.get(); }
	const std::vector<PSOBucket>& GetBuckets() { return m_buckets; }
	inline int GetPSOIndex(const std::string& psoName) { return m_psoList->IndexOf(psoName); }
	bool GetPsoEnabled(int psoIndex) { return m_passEnabled[psoIndex]; }
	inline void SetPsoEnabled(const std::string& psoName, bool bEnabled) { m_passEnabled[GetPSOIndex(psoName)] = bEnabled; }

	void PrepareRender(_In_ UploadContext* uploadContext, _In_ DescriptorAllocator* descriptorAllocator, const CameraConstants& cameraData, const LightBlobView& lightData, uint32_t frameIndex);
	void RenderFrame(ID3D12GraphicsCommandList* cmd);
	bool IsDynamicRegistered(IDrawable* drawable, const std::string& psoName);
	bool RegisterStatic(IDrawable* drawable, const std::string& psoName, uint32_t frameIndex);
	bool RegisterDynamic(IDrawable* drawable, const std::string& psoName);
	bool UnRegisterStatic(IDrawable* drawable, const std::string& psoName);
	bool UnRegisterDynamic(IDrawable* drawable, const std::string& psoName);
	bool UpdateDynamic(IDrawable* drawable, const GeometryData& data);

#ifdef _DEBUG
	void SetWireViewEnabled(bool enable) { m_wireViewEnabled = enable; }
#endif // _DEBUG

private:
	RenderSystemInitInfo m_info;

	std::unique_ptr<PSOList> m_psoList;
	std::unique_ptr<BundleRecorder> m_bundleRecorder;

	std::vector<PSOBucket> m_buckets;
	std::vector<bool> m_passEnabled;

	BufferHandle m_cameraBuf{};
	BufferHandle m_lightsBuf{};
	D3D12_GPU_DESCRIPTOR_HANDLE m_lightsGpu{};

#ifdef _DEBUG
	bool m_wireViewEnabled = false;
#endif // _DEBUG

};

