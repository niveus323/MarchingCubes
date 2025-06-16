#pragma once
#include "Core/DataStructures/Data.h"
#include "Core/Rendering/BundleRecorder.h"
#include "Core/DataStructures/ShaderTypes.h"
using Microsoft::WRL::ComPtr;

class MeshBuffer :public IDrawable
{
public:
	void CreateBuffers(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, const MeshData& data);
	void CreateObjectConstantBuffer(ID3D12Device* device, const ObjectConstants& world);
	void UpdateObjectConstantBuffer(const DirectX::XMFLOAT3& position, const DirectX::XMVECTOR& quat, const DirectX::XMFLOAT3& scale);
	void Draw(ID3D12GraphicsCommandList* cmd) const;
	UINT GetIndexCount() const override { return m_indexCount; };
	D3D12_PRIMITIVE_TOPOLOGY GetTopology() const { return m_topology; };

private:
	// 읽기 전용 버퍼와 쓰기 전용 버퍼를 분리하면 다음의 효과를 얻을 수 있음.
	// 1. CPU->GPU 업로드 시 Upload Heap을 사용하면 Map과 memcpy 호출로 데이터를 쓸 수 있음.
	// 2. GPU 렌더링 시 GPU는 Default Heap을 가장 빨리 읽음.
	// 3. CPU가 데이터를 쓰는 동안 GPU가 사용 중인 리소스에 접근 대기하는 대기 시간(파이프라인 동기화 시간)을 없앨 수 있음.
	// CPU Upload Only Buffer
	ComPtr<ID3D12Resource> m_vertexUploadBuffer;
	ComPtr<ID3D12Resource> m_indexUploadBuffer;
	// GPU Read Only Buffer
	ComPtr<ID3D12Resource> m_vertexBuffer;
	ComPtr<ID3D12Resource> m_indexBuffer;
	D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;
	D3D12_INDEX_BUFFER_VIEW m_indexBufferView;

	D3D12_PRIMITIVE_TOPOLOGY m_topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	UINT m_indexCount;

	// Object Constants Buffer
	ComPtr<ID3D12Resource> m_objectConstantsBuffer;
	UINT8* m_mappedObjectCB;
};

class Mesh
{
public:
	// NOTE : cmdList가 open-for-recording 상태여야 함.
	void Upload(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, const MeshData& meshData);
	void Update();
	
	void SetPosition(const DirectX::XMFLOAT3& pos);
	void SetRotation(const DirectX::XMFLOAT3& quat);
	void SetScale(const DirectX::XMFLOAT3& scale);

	void Move(const DirectX::XMFLOAT3& delta);
	void Rotate(const DirectX::XMVECTOR& deltaQuat);
	void Scale(const DirectX::XMFLOAT4& scaleFactor);

	void SetColor(const DirectX::XMFLOAT4& color);

public:
	MeshBuffer m_buffer;
	DirectX::XMFLOAT3 m_position = { 0.0f, 0.0f, 0.0f };
	DirectX::XMFLOAT3 m_rotation = { 0.0f, 0.0f, 0.0f };
	DirectX::XMFLOAT3 m_scale = { 1.0f, 1.0f, 1.0f };
	DirectX::XMFLOAT4 m_color = { 1.0f, 1.0f, 1.0f, 1.0f };
};

