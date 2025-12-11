#pragma once
#include "PSO/PSOList.h"
#include "BundleRecorder.h"
#include "Core/Geometry/Mesh/Mesh.h"
#include "Core/Rendering/UploadContext.h"
#include "Memory/GpuAllocator.h"
#include "Memory/StaticBufferRegistry.h"

struct PSOBucket
{
	std::vector<RenderItem> renderItems;
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

	void PrepareRender(_In_ UploadContext* uploadContext, _In_ DescriptorAllocator* descriptorAllocator, const CameraConstants& cameraData, const LightBlobView& lightData, uint32_t frameIndex);
	void RenderFrame(_In_ ID3D12GraphicsCommandList* cmd, _In_ UploadContext* uploadContext);
	bool SubmitRenderItem(const RenderItem& item, std::string_view psoName);

	PSOList* GetPSOList() { return m_psoList.get(); }
	BundleRecorder* GetBundleRecorder() { return m_bundleRecorder.get(); }
	const std::vector<PSOBucket>& GetBuckets() { return m_buckets; }
	inline int GetPSOIndex(std::string_view psoName) { return m_psoList->IndexOf(psoName); }

	// PSO Override & Extension
	const auto& GetPsoOverrides() { return m_psoOverrides; }
	const auto& GetPsoExtensions() { return m_psoExtensions; }
	bool IsOverrideActive(const std::string& from, const std::string& to) 
	{
		auto it = m_psoOverrides.find(from);
		return (it != m_psoOverrides.end() && it->second == to);
	}
	bool IsPSOOverridden(const std::string& from) { return m_psoOverrides.find(from) != m_psoOverrides.end(); }
	bool IsPSOExtended(const std::string& base, const std::string& extent) 
	{
		auto range = m_psoExtensions.equal_range(base);
		for (auto it = range.first; it != range.second; ++it) 
		{
			if (it->second == extent) return true;
		}
		return false;
	}
	void TogglePSOExtension(const std::string& targetBase, const std::string& extName) 
	{
		IsPSOExtended(targetBase, extName) ? RemovePSOExtension(targetBase, extName) : AddPSOExtension(targetBase, extName);
	}
	void SetPSOOverride(const std::string& from, const std::string& to) { m_psoOverrides[from] = to; }
	void AddPSOExtension(const std::string& base, const std::string& extent) { m_psoExtensions.insert({ base, extent }); }
	void ResetPSOOverride(const std::string& from) { m_psoOverrides.erase(from); }
	void ResetPSOExtension(const std::string& from) { m_psoExtensions.erase(from); }
	void RemovePSOExtension(const std::string& base, const std::string& extent);
	void ClearPSOOverrides() { m_psoOverrides.clear(); }
	void ClearPSOExtionstions() { m_psoExtensions.clear(); }
	void ClearPSORules() { m_psoOverrides.clear(); m_psoExtensions.clear(); }

private:
	void SubmitToBucket(std::string_view psoName, const RenderItem& item);

private:
	RenderSystemInitInfo m_info;

	std::unique_ptr<PSOList> m_psoList;
	std::unique_ptr<BundleRecorder> m_bundleRecorder;

	std::vector<PSOBucket> m_buckets;
	std::unordered_map<std::string, std::string> m_psoOverrides;
	std::unordered_multimap< std::string, std::string> m_psoExtensions;

	BufferHandle m_cameraBuf{};
	BufferHandle m_lightsBuf{};
	D3D12_GPU_DESCRIPTOR_HANDLE m_lightsGpu{};
};

