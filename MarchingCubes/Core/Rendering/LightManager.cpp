#include "pch.h"
#include "LightManager.h"

LightManager::LightManager(ID3D12Device* device, UINT maxLights, UINT rootParamIndex) :
	//m_mappedData(nullptr),
	m_uploadBufferSize(0),
	m_maxLights(maxLights),
	m_rootParamIndex(rootParamIndex)
{
	m_lights.reserve(maxLights);

	m_headerSizeAligned = static_cast<UINT>(sizeof(LightConstantsHeader));

	UINT lightsSize = sizeof(Light) * maxLights;
	m_uploadBufferSize = AlignUp(m_headerSizeAligned + lightsSize, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);

	CreateUploadBuffer(device);
}

LightManager::~LightManager()
{
	if (m_uploadBuffer && m_mappedBase)
	{
		m_uploadBuffer->Unmap(0, nullptr);
	}
}

void LightManager::AddDirectional(const DirectX::XMFLOAT3& dir, const DirectX::XMFLOAT3& radiance)
{
	if (m_lights.size() >= m_maxLights) return;

	Light data{};
	data.type = ELightType::Directional;
	data.radiance = radiance;
	XMVECTOR d = XMVector3Normalize(XMLoadFloat3(&dir));
	XMStoreFloat3(&data.dirOrPos, d);
	m_lights.emplace_back(data);
}

void LightManager::AddPoint(const DirectX::XMFLOAT3& pos, const DirectX::XMFLOAT3& radiance, float range)
{
	if (m_lights.size() >= m_maxLights) return;

	Light data{};
	data.type = ELightType::Point;
	data.radiance = radiance;
	data.dirOrPos = pos;
	data.rangeOrPadding = range;
	m_lights.emplace_back(data);
}

void LightManager::AddSpot(const DirectX::XMFLOAT3& pos, const DirectX::XMFLOAT3& dir, const DirectX::XMFLOAT3& radiance, float innerCos, float outerCos, float range)
{
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

void LightManager::Update()
{
	UINT count = std::min<UINT>((UINT)m_lights.size(), m_maxLights);
	m_header->lightCounts = count;

	std::memcpy(m_lightsPtr, m_lights.data(), count * sizeof(Light));
}

void LightManager::BindConstant(ID3D12GraphicsCommandList* cmdList)
{
	cmdList->SetGraphicsRootConstantBufferView(m_rootParamIndex, m_uploadBuffer->GetGPUVirtualAddress());
}

void LightManager::CreateUploadBuffer(ID3D12Device* device)
{
	ThrowIfFailed(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(m_uploadBufferSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&m_uploadBuffer)
	));
	NAME_D3D12_OBJECT(m_uploadBuffer);


	ThrowIfFailed(m_uploadBuffer->Map(0, nullptr, reinterpret_cast<void**>(&m_mappedBase)));

	m_header = reinterpret_cast<LightConstantsHeader*>(m_mappedBase);
	m_lightsPtr = reinterpret_cast<Light*>(m_mappedBase + m_headerSizeAligned);
}

