#pragma once
#include "Core/DataStructures/Data.h"
#include "Core/DataStructures/ShaderTypes.h"
using Microsoft::WRL::ComPtr;

class LightManager
{
public:
	LightManager(ID3D12Device* device, uint32_t rootParamIndex);
	~LightManager();

	void AddDirectional(const DirectX::XMFLOAT3& dir, const DirectX::XMFLOAT3& radiance);
	void AddPoint(const DirectX::XMFLOAT3& pos, const DirectX::XMFLOAT3& radiance, float range);
	void AddSpot(const DirectX::XMFLOAT3& pos, const DirectX::XMFLOAT3& dir, const DirectX::XMFLOAT3& radiance, float innerCos, float outerCos, float range);
	
	LightBlobView BuildLightConstants() const;

	// 체크용 임시 함수
	void SetDirection(const DirectX::XMFLOAT3& dir) { m_lights[0].dirOrPos = dir; };

	uint32_t GetLightCount() const { return (uint32_t)m_lights.size(); }
	
private:
	std::vector<Light> m_lights;
	mutable std::vector<uint8_t> m_scratch; // Caching Buffer
};

