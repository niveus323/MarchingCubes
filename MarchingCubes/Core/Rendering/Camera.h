#pragma once
using namespace DirectX;

class InputState;

class Camera
{
public:
	Camera(float aspect, float fov = XM_PI/4, float zNear = 0.1f, float zFar = 1000.0f);
	~Camera() = default;

	XMMATRIX GetViewMatrix() const { return XMLoadFloat4x4(&m_viewMatrix); }
	XMMATRIX GetProjMatrix() const { return XMLoadFloat4x4(&m_projMatrix); }
	XMMATRIX GetViewProjMatrix() const { return GetViewMatrix() * GetProjMatrix(); }

	void SetPosition(float x, float y, float z);
	void SetTarget(float x, float y, float z);
	void SetUp(float x, float y, float z);
	void SetLens(float fov, float aspect, float zNear, float zFar);

	void UpdateViewMatrix();
	void UpdateProjMatrix();

	// Constant Buffer 관련 함수
	void CreateConstantBuffer(ID3D12Device* device);
	void UpdateConstantBuffer();
	void BindConstantBuffer(ID3D12GraphicsCommandList* cmdList, UINT rootIndex);

	// 카메라 이동 & 회전
	void Rotate(float deltaX, float deltaY);
	void Move(const InputState& input, float deltaTime);

private:
	XMFLOAT3 m_position;
	XMFLOAT3 m_rightDir;
	XMFLOAT3 m_upDir;
	XMFLOAT3 m_targetPos;

	float m_nearZ;
	float m_farZ;
	float m_aspect;
	float m_fov;

	XMFLOAT4X4 m_viewMatrix;
	XMFLOAT4X4 m_projMatrix;

	// Constant Buffer 관련 리소스
	Microsoft::WRL::ComPtr<ID3D12Resource> m_cameraBuffer;
	UINT8* m_mappedDataCB;

	// 카메라 이동 & 회전
	float m_yaw = 0.0f;
	float m_pitch = 0.0f;
	float m_moveSpeed = 5.0f;
	float m_mouseSensitivity = 0.005f;
};

