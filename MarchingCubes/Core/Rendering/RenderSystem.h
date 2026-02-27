#pragma once
#include "PSO/PSOList.h"
#include "BundleRecorder.h"

class MeshRegistry;
class TextureRegistry;
class MaterialRegistry;

class RenderSystem
{
public:
	RenderSystem(const std::vector<D3D12_INPUT_ELEMENT_DESC>& inputElems, const std::vector<std::wstring>& psoFiles);
	~RenderSystem();

	void SyncGpu(ID3D12GraphicsCommandList* cmd);
	void PrepareRender(const CameraConstants& cameraData, const LightBlobView& lightData, uint32_t frameIndex);
	void RenderFrame(ID3D12GraphicsCommandList* cmd);
	bool SubmitRenderItem(const RenderItem& item, std::string_view psoName);

	PSOList* GetPSOList() { return m_psoList.get(); }
	BundleRecorder* GetBundleRecorder() { return m_bundleRecorder.get(); }
	
	// Registry Getter
	MeshRegistry* GetMeshRegistry() const { return m_meshRegistry.get(); }
	TextureRegistry* GetTextureRegistry() const { return m_textureRegistry.get(); }
	MaterialRegistry* GetMaterialRegistry() const { return m_materialRegistry.get(); }

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
	
	const D3D12_VIEWPORT& GetViewport() const { return m_viewport; }
	const D3D12_RECT& GetScissorRect() const { return m_scissorRect; }
	void SetViewport(float x, float y, float width, float height);
	ID3D12Resource* GetOutputTargetRes() const { return m_currentRenderTarget; }
	D3D12_CPU_DESCRIPTOR_HANDLE GetOutputDSV() const { return m_currentDSV; }
	D3D12_CPU_DESCRIPTOR_HANDLE GetOutputRTV() const { return m_currentRTV; }
	void SetOutputTarget(ID3D12Resource* renderTarget, D3D12_CPU_DESCRIPTOR_HANDLE rtv, D3D12_CPU_DESCRIPTOR_HANDLE dsv);

private:
	bool SubmitToQueue(std::string_view psoName, const RenderItem& item);
	uint64_t GenerateSortKey(uint16_t rsIndex, uint16_t psoIndex, const RenderItem& item);

private:
	std::vector<D3D12_INPUT_ELEMENT_DESC> m_inputElements;
	std::vector<std::wstring> m_psoFiles;

	std::unique_ptr<PSOList> m_psoList;
	std::unique_ptr<BundleRecorder> m_bundleRecorder;

	struct DrawCommand
	{
		uint64_t sortKey;
		uint32_t psoIndex;
		RenderItem item;
	};
	std::vector<DrawCommand> m_renderQueue; //TODO : 투명 오브젝트 적용 시 transparentQueue를 별도로 둘 것(Depth 우선인 별도의 기준 필요)

	std::unordered_map<std::string, std::string> m_psoOverrides;
	std::unordered_multimap< std::string, std::string> m_psoExtensions;

	BufferHandle m_cameraBuf{};
	BufferHandle m_lightsBuf{};

	// Registry
	std::unique_ptr<TextureRegistry> m_textureRegistry;
	std::unique_ptr<MaterialRegistry> m_materialRegistry;
	std::unique_ptr<MeshRegistry> m_meshRegistry;

	// Screen
	D3D12_VIEWPORT m_viewport{};
	D3D12_RECT m_scissorRect{};

	ID3D12Resource* m_currentRenderTarget = nullptr;
	D3D12_CPU_DESCRIPTOR_HANDLE m_currentRTV;
	D3D12_CPU_DESCRIPTOR_HANDLE m_currentDSV;
};

