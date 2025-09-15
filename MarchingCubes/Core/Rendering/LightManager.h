#pragma once
#include "Core/DataStructures/ShaderTypes.h"
using Microsoft::WRL::ComPtr;

class LightManager
{
public:
	LightManager(ID3D12Device* device, UINT maxLights, UINT rootParamIndex);
	~LightManager();

	void AddDirectional(const DirectX::XMFLOAT3& dir, const DirectX::XMFLOAT3& radiance);
	void AddPoint(const DirectX::XMFLOAT3& pos, const DirectX::XMFLOAT3& radiance, float range);
	void AddSpot(const DirectX::XMFLOAT3& pos, const DirectX::XMFLOAT3& dir, const DirectX::XMFLOAT3& radiance, float innerCos, float outerCos, float range);
	void Update();
	void BindConstant(ID3D12GraphicsCommandList* cmdList);

	// 체크용 임시 함수
	void SetDirection(const DirectX::XMFLOAT3& dir) { m_lights[0].dirOrPos = dir; };

	uint32_t GetLightCount() const { return (uint32_t)m_lights.size(); }
	
private:
	void CreateUploadBuffer(ID3D12Device* device);

private:
	ComPtr<ID3D12Resource> m_uploadBuffer;
	//LightConstants* m_mappedData;
	UINT m_uploadBufferSize;

	std::vector<Light> m_lights;
	const UINT m_maxLights;
	const UINT m_rootParamIndex;

	uint8_t* m_mappedBase = nullptr;
	LightConstantsHeader* m_header = nullptr;
	Light* m_lightsPtr = nullptr;
	UINT m_headerSizeAligned = 0;
};

