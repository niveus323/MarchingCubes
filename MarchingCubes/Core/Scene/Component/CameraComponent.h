#pragma once
#include "Component.h"
#include "Core/DataStructures/ShaderTypes.h"

class CameraComponent : public Component
{
	REFLECT_GENERATED_BODY(CameraComponent)
public:
	virtual ~CameraComponent() = default;
	
	CameraConstants GetCameraConstants() const;

	XMMATRIX GetViewMatrix() const;
	XMMATRIX GetProjMatrix() const { return DirectX::XMMatrixPerspectiveFovLH(m_fov, m_aspect, m_nearZ, m_farZ); }
	XMMATRIX GetViewProjMatrix() const { return GetViewMatrix() * GetProjMatrix(); }

	XMVECTOR GetForwardVector() const;
	XMVECTOR GetRightVector() const;
	XMVECTOR GetUpVector() const;

	float GetViewportWidth() const { return m_viewportWidth; }
	float GetViewportHeight() const { return m_viewportHeight; }

	void SetViewport(float width, float height)
	{
		m_viewportWidth = width;
		m_viewportHeight = height;
		m_aspect = m_viewportWidth / m_viewportHeight;
	}

	void SetLens(float fov, float zNear, float zFar)
	{
		m_fov = fov;
		m_nearZ = zNear;
		m_farZ = zFar;
	}

private:
	float m_viewportWidth;
	float m_viewportHeight;
	float m_fov = XM_PIDIV4;
	float m_aspect = 1.777f;
	float m_nearZ = 0.1f;
	float m_farZ = 1000.0f;
};

