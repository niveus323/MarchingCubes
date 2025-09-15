#include "pch.h"
#include "Camera.h"
#include <algorithm>
#include "Core/DataStructures/Data.h"
#include "Core/Input/InputState.h"
#include "Core/DataStructures/ShaderTypes.h"
#include "Material.h"

Camera::Camera(float viewportWidth, float viewportHeight, float fov, float zNear, float zFar)
	: m_position(0.0f, 0.0f, -100.0f),
	m_rightDir(1.0f, 0.0f, 0.0f),
	m_upDir(0.0f, 1.0f, 0.0f),
	m_targetPos(0.0f, 0.0f, 0.0f),
	m_viewportWidth(viewportWidth),
	m_viewportHeight(viewportHeight),
	m_nearZ(zNear),
	m_farZ(zFar),
	m_aspect(viewportWidth / viewportHeight),
	m_fov(fov),
	m_mappedDataCB(nullptr)
{
	UpdateViewMatrix();
	UpdateProjMatrix();
}

Camera::~Camera()
{
	if (m_mappedDataCB)
	{
		m_cameraBuffer->Unmap(0, nullptr);
		m_mappedDataCB = nullptr;
	}
	m_cameraBuffer.Reset();
}

void Camera::SetPosition(float x, float y, float z)
{
	m_position = XMFLOAT3(x, y, z);
}

void Camera::SetTarget(float x, float y, float z)
{
	m_targetPos = XMFLOAT3(x, y, z);
}

void Camera::SetUp(float x, float y, float z)
{
	m_upDir = XMFLOAT3(x, y, z);
}

void Camera::SetLens(float fov, float aspect, float zNear, float zFar)
{
	m_fov = fov;
	m_aspect = aspect;
	m_nearZ = zNear;
	m_farZ = zFar;
	UpdateProjMatrix();
}

void Camera::UpdateViewMatrix()
{
	// === Basis Àç°è»ê ===
	XMVECTOR pos = XMLoadFloat3(&m_position);
	XMVECTOR target = XMLoadFloat3(&m_targetPos);
	XMVECTOR forward = XMVector3Normalize(XMVectorSubtract(target, pos));
	XMVECTOR worldUp = XMVectorSet(0, 1, 0, 0);

	float dot = XMVectorGetX(XMVector3Dot(worldUp, forward));
	if (fabs(dot) > 0.999f)
	{
		worldUp = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
	}

	XMVECTOR right = XMVector3Normalize(XMVector3Cross(worldUp, forward));
	XMVECTOR up = XMVector3Cross(forward, right);

	XMStoreFloat3(&m_rightDir, right);
	XMStoreFloat3(&m_upDir, up);

	XMMATRIX viewMatrix = XMMatrixLookAtLH(XMLoadFloat3(&m_position), XMLoadFloat3(&m_targetPos), XMLoadFloat3(&m_upDir));
	XMStoreFloat4x4(&m_viewMatrix, viewMatrix);
}

void Camera::UpdateProjMatrix()
{
	XMStoreFloat4x4(&m_projMatrix, XMMatrixPerspectiveFovLH(m_fov, m_aspect, m_nearZ, m_farZ));
}

void Camera::CreateConstantBuffer(ID3D12Device* device)
{
	static const UINT cameraBufferSize = AlignUp(sizeof(CameraConstants), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);

	ThrowIfFailed(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(cameraBufferSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&m_cameraBuffer)
	));
	NAME_D3D12_OBJECT(m_cameraBuffer);

	// Map & CreateUploadBuffer Constant Buffer
	CD3DX12_RANGE readRAnge(0, 0);
	ThrowIfFailed(m_cameraBuffer->Map(0, &readRAnge, reinterpret_cast<void**>(&m_mappedDataCB)));
}

void Camera::UpdateConstantBuffer()
{
	CameraConstants cb{};
	XMMATRIX vp = GetViewProjMatrix();
	XMStoreFloat4x4(&cb.viewProjMatrix, XMMatrixTranspose(vp));
	cb.cameraPosition = m_position;
	memcpy(m_mappedDataCB, &cb, sizeof(cb));
}

void Camera::BindConstantBuffer(ID3D12GraphicsCommandList* cmdList, UINT rootIndex)
{
	cmdList->SetGraphicsRootConstantBufferView(rootIndex, m_cameraBuffer->GetGPUVirtualAddress());
}

void Camera::Rotate(float deltaX, float deltaY)
{
	m_yaw += deltaX * m_mouseSensitivity;
	m_pitch += deltaY * m_mouseSensitivity;

	const float limit = XM_PIDIV2 - 0.0f;
	m_pitch = std::clamp(m_pitch, -limit, limit);

	XMVECTOR forward = XMVectorSet(cosf(m_pitch) * sinf(m_yaw), sinf(m_pitch), cosf(m_pitch) * cosf(m_yaw), 0.0f);
	XMVECTOR position = XMLoadFloat3(&m_position);
	XMVECTOR target = XMVectorAdd(position, forward);
	XMStoreFloat3(&m_targetPos, target);

	XMVECTOR up = XMVectorSet(0, 1, 0, 0);
	XMVECTOR right = XMVector3Normalize(XMVector3Cross(up, XMVectorSubtract(target, position)));
	XMStoreFloat3(&m_rightDir, right);

	XMStoreFloat3(&m_upDir, XMVector3Normalize(XMVector3Cross(XMLoadFloat3(&m_rightDir), XMVectorSubtract(target, position))));
}

void Camera::Move(const InputState& input, float deltaTime)
{
	float velocity = m_moveSpeed * deltaTime;

	XMVECTOR pos = XMLoadFloat3(&m_position);
	XMVECTOR forward = XMVector3Normalize(XMVectorSubtract(XMLoadFloat3(&m_targetPos), pos));
	XMVECTOR right = XMLoadFloat3(&m_rightDir);
	XMVECTOR up = XMLoadFloat3(&m_upDir);

	if (input.IsPressed(ActionKey::MoveForward)) pos = XMVectorAdd(pos, forward * velocity);
	if (input.IsPressed(ActionKey::MoveBackward)) pos = XMVectorSubtract(pos, forward * velocity);
	if (input.IsPressed(ActionKey::MoveLeft)) pos = XMVectorSubtract(pos, right * velocity);
	if (input.IsPressed(ActionKey::MoveRight)) pos = XMVectorAdd(pos, right * velocity);
	if (input.IsPressed(ActionKey::MoveUp)) pos = XMVectorAdd(pos, up * velocity);
	if (input.IsPressed(ActionKey::MoveDown)) pos = XMVectorSubtract(pos, up * velocity);

	XMStoreFloat3(&m_position, pos);
	XMStoreFloat3(&m_targetPos, XMVectorAdd(pos, forward));
}
