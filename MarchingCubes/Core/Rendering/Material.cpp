#include "pch.h"
#include "Material.h"

Material::~Material()
{
    if (m_mappedData)
    {
        m_buffer->Unmap(0, nullptr);
        m_mappedData = nullptr;
    }
    m_buffer.Reset();
}

void Material::CreateConstantBuffer(ID3D12Device* device)
{
    UINT size = AlignUp(sizeof(MaterialConstants), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
    ThrowIfFailed(device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(size),
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(m_buffer.ReleaseAndGetAddressOf())
    ));
    NAME_D3D12_OBJECT_ALIAS(m_buffer, L"Material");

    ThrowIfFailed(m_buffer->Map(0, nullptr, reinterpret_cast<void**>(&m_mappedData)));
    memcpy(m_mappedData, &m_cb, sizeof(m_cb));
}

void Material::Update(float deltaTime)
{
    memcpy(m_mappedData, &m_cb, sizeof(m_cb));
}

void Material::BindConstant(ID3D12GraphicsCommandList* cmdList) const
{
    cmdList->SetGraphicsRootConstantBufferView(2, m_buffer->GetGPUVirtualAddress());
}