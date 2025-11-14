#include "pch.h"
#include "LightManager.h"

LightManager::LightManager(ID3D12Device* device, uint32_t rootParamIndex)
{
	m_lights.reserve(kMaxLights);

	//m_headerSizeAligned = static_cast<uint32_t>(sizeof(LightConstantsHeader));

	//uint32_t lightsSize = sizeof(Light) * maxLights;
	//m_uploadBufferSize = AlignUp(m_headerSizeAligned + lightsSize, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
	//
	//CreateUploadBuffer(device);
}

LightManager::~LightManager()
{
	//if (m_uploadBuffer && m_mappedBase)
	//{
	//	m_uploadBuffer->Unmap(0, nullptr);
	//}
}

void LightManager::AddDirectional(const DirectX::XMFLOAT3& dir, const DirectX::XMFLOAT3& radiance)
{
	if (m_lights.size() >= kMaxLights) return;

	Light data{};
	data.type = ELightType::Directional;
	data.radiance = radiance;
	XMVECTOR d = XMVector3Normalize(XMLoadFloat3(&dir));
	XMStoreFloat3(&data.dirOrPos, d);
	m_lights.emplace_back(data);
}

void LightManager::AddPoint(const DirectX::XMFLOAT3& pos, const DirectX::XMFLOAT3& radiance, float range)
{
	if (m_lights.size() >= kMaxLights) return;

	Light data{};
	data.type = ELightType::Point;
	data.radiance = radiance;
	data.dirOrPos = pos;
	data.rangeOrPadding = range;
	m_lights.emplace_back(data);
}

void LightManager::AddSpot(const DirectX::XMFLOAT3& pos, const DirectX::XMFLOAT3& dir, const DirectX::XMFLOAT3& radiance, float innerCos, float outerCos, float range)
{
	if (m_lights.size() >= kMaxLights) return;

	Light data{};
	data.type = ELightType::Spot;
	data.radiance = radiance;
	data.dirOrPos = pos;
	data.rangeOrPadding = range;
	XMVECTOR d = XMVector3Normalize(XMLoadFloat3(&dir));
	XMStoreFloat3(&data.spotDir, d);
	data.spotInnerCos = innerCos;
	m_lights.emplace_back(data);
}

LightBlobView LightManager::BuildLightConstants() const
{
	// 1) 라이트 개수 계산
	uint32_t count = (uint32_t)m_lights.size(); // 여기서 m_lights는 "씬의 현재 라이트" 목록

	// 2) 필요한 총 바이트 수 계산
	size_t totalBytes =
		sizeof(LightConstantsHeader) +
		sizeof(Light) * count;

	// 3) 내부 임시 버퍼 사이즈 맞추기
	m_scratch.resize(totalBytes);

	// 4) header 채우기
	LightConstantsHeader header{};
	header.lightCounts = count;
	// padding 등도 세팅

	// 5) scratch[0..headerSize) 에 header 복사
	std::memcpy(m_scratch.data(), &header, sizeof(LightConstantsHeader));

	// 6) 그 다음 라이트 배열 채우기
	uint8_t* dstLights = m_scratch.data() + sizeof(LightConstantsHeader);
	for (uint32_t i = 0; i < count; ++i) 
	{
		std::memcpy(dstLights + i * sizeof(Light), &m_lights[i], sizeof(Light));
	}

	// 7) 뷰 리턴
	LightBlobView view{};
	view.data = m_scratch.data();
	view.size = totalBytes;
	return view;
}
