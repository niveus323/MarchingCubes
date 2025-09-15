#pragma once
#include "Core/DataStructures/Data.h"
#include "Core/DataStructures/ShaderTypes.h"
#include "Core/Rendering/BundleRecorder.h"
#include "Core/Rendering/Material.h"
#include <Core/Rendering/Camera.h>
#include <DirectXCollision.h>
using Microsoft::WRL::ComPtr;

// TODO : ObjectConstants + Material을 상위 개념(Model 혹은 Renderable)로 옮기기. < SubMesh와 Material 슬롯 처리 목적 >

// 순수 IB/VB 래퍼
class MeshBuffer
{
public:
	MeshBuffer();
	~MeshBuffer();
	void CreateBuffers(ID3D12Device* device, size_t maxVertices, size_t maxIndices);
	void CreateBuffers(ID3D12Device* device, const MeshData& data);
	
	void StageBuffers(const MeshData& data); // CPU->Upload Heap 복사
	void CommitBuffers(ID3D12GraphicsCommandList* cmdList, const MeshData& data); // Upload Heap -> Default Heap 복사
	void ResizeIfNeededAndCommit(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, const MeshData& data);
	
	void SetDeletionSink(std::vector<ComPtr<ID3D12Resource>>* sink);

	void CreateObjectConstantBuffer(ID3D12Device* device, const ObjectConstants& world);
	void UpdateObjectConstantBuffer(const ObjectConstants& cb);

	void BindObjectConstants(ID3D12GraphicsCommandList* cmdList) const;

	void Draw(ID3D12GraphicsCommandList* cmdList) const;
	UINT GetVertexCount() const { return m_vertexCount; }
	UINT GetIndexCount() const { return m_indexCount; }
	D3D12_PRIMITIVE_TOPOLOGY GetTopology() const { return m_topology; };
	void SetTopology(D3D12_PRIMITIVE_TOPOLOGY topology) { m_topology = topology; };
	void ClearCounts() { m_indexCount = 0; }; // 자주 갱신되는 MeshBuffer에 대해 바로 버퍼를 해제하지 않는 것이 효율적이라고 판단된다면 count만 clear하도록 한다.

private:
	void Initialize(ID3D12Device* device, UINT vertexBufferSize, UINT indexBufferSize);
	
private:
	// 읽기 전용 버퍼와 쓰기 전용 버퍼를 분리하면 다음의 효과를 얻을 수 있음.
	// 1. CPU->GPU 업로드 시 Upload Heap을 사용하면 Map과 memcpy 호출로 데이터를 쓸 수 있음.
	// 2. GPU 렌더링 시 GPU는 Default Heap을 가장 빨리 읽음.
	// 3. CPU가 데이터를 쓰는 동안 GPU가 사용 중인 리소스에 접근 대기하는 대기 시간(파이프라인 동기화 시간)을 없앨 수 있음.
	
	// CPU Upload Only Buffer
	ComPtr<ID3D12Resource> m_vertexUploadBuffer, m_indexUploadBuffer;
	
	// GPU Read Only Buffer
	ComPtr<ID3D12Resource> m_vertexBuffer, m_indexBuffer;

	D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView{};
	D3D12_INDEX_BUFFER_VIEW m_indexBufferView{};

	D3D12_PRIMITIVE_TOPOLOGY m_topology;
	// Real Count
	UINT  m_vertexCount, m_indexCount;
	// Reserved Count
	UINT m_reservedVertexCount, m_reservedIndexCount;

	// Object Constants Buffer
	ComPtr<ID3D12Resource> m_objectCB;
	UINT8* m_mappedObjectCB;

	// ResourceBarrier
	bool m_justInitailized;

	// Deletion Sink
	std::vector<ComPtr<ID3D12Resource>>* m_deletionSink = nullptr;
};

class Mesh :public IDrawable
{
public:
	Mesh() = default;

	// IDrawable을(를) 통해 상속됨
	void Draw(ID3D12GraphicsCommandList* cmdList) const override;
	void SetConstantsBuffers(ID3D12GraphicsCommandList* cmdList) const override;

	void Initialize(ID3D12Device* device, const MeshData& meshData);
	void Update(float deltaTime);
	void Upload(ID3D12Device* device);
	void UpdateConstants();

	void StageBuffers();
	void CommitBuffers(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList);

	// NOTE : cmdList가 open-for-recording 상태여야 함.
	void UpdateData(const MeshData& meshData);
	
	// Deliver Deletion Sink
	void SetDeletionSink(std::vector<ComPtr<ID3D12Resource>>* sink) { m_buffer.SetDeletionSink(sink); }

	const MeshBuffer& GetMeshBuffer() const { return m_buffer; }
	const MeshData& GetMeshData() const { return m_data; }
	const std::vector<DirectX::BoundingBox>& GetBounds() const { return m_triBounds; }
	DirectX::XMMATRIX GetWorldMatrix() const;
	DirectX::XMMATRIX GetWorldInvMatrix() const;

	void SetMeshData(const MeshData& meshData) { m_data = meshData; }
	void SetMeshData(MeshData&& meshData) { m_data = std::move(meshData); }
	void SetMaterial(std::shared_ptr<Material> mat) { m_material = std::move(mat); }

	DirectX::XMFLOAT3 GetPosition() const { return m_position; }
	void SetPosition(const DirectX::XMFLOAT3& pos) { m_position = pos; }
	DirectX::XMFLOAT3 GetRotation() const { return m_rotation; }
	void SetRotation(const DirectX::XMFLOAT3& rotation) { m_rotation = rotation; }
	void SetRotation(const DirectX::XMVECTOR& quat) { m_rotation = ToEulerFromQuat(quat); }
	DirectX::XMFLOAT3 GetScale() const { return m_scale; }
	void SetScale(const DirectX::XMFLOAT3& scale) { m_scale = scale; }
	// TODO : PBR로 수정하였으므로 전용 Material 혹은 shader 적용하도록 수정.
	void SetColor(const DirectX::XMFLOAT4& color);

	void Move(const DirectX::XMFLOAT3& delta);
	void Rotate(const DirectX::XMVECTOR& deltaQuat);
	void Scale(const DirectX::XMFLOAT4& scaleFactor);
	void BuildTriBounds();
private:
	void FillObjectConstants();

private:
	MeshBuffer m_buffer;
	MeshData m_data;
	ObjectConstants m_objectCB{};
	std::shared_ptr<Material> m_material; // Material은 App 클래스에서 공유받아 bind만 해준다.

	DirectX::XMFLOAT3 m_position = { 0.0f, 0.0f, 0.0f };
	DirectX::XMFLOAT3 m_rotation = { 0.0f, 0.0f, 0.0f };
	DirectX::XMFLOAT3 m_scale = { 1.0f, 1.0f, 1.0f };

	std::vector<DirectX::BoundingBox> m_triBounds;
};

