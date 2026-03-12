#pragma once
#include "TransformableComponent.h"
#include "Core/DataStructures/ShaderTypes.h"

class CameraComponent : public TransformableComponent
{
	REFLECT_GENERATED_BODY(CameraComponent)
public:
	virtual void Init() override;
	CameraConstants GetCameraConstants(float rtWidth, float rtHegiht) const;

	XMMATRIX GetViewMatrix() const;
	XMMATRIX GetProjMatrix() const { return DirectX::XMMatrixPerspectiveFovLH(m_fov, m_aspect, m_nearZ, m_farZ); }
	XMMATRIX GetViewProjMatrix() const { return GetViewMatrix() * GetProjMatrix(); }

	XMVECTOR GetForwardVector() const;
	XMVECTOR GetRightVector() const;
	XMVECTOR GetUpVector() const;

	/*float GetViewportWidth() const { return m_viewportWidth; }
	float GetViewportHeight() const { return m_viewportHeight; }*/

	float GetViewportRectX() const { return m_viewportRectX; }
	float GetViewportRectY() const { return m_viewportRectY; }
	float GetViewportRectW() const { return m_viewportRectW; }
	float GetViewportRectH() const { return m_viewportRectH; }

	void SetViewportRect(float x, float y, float w, float h)
	{
		m_viewportRectX = x; 
		m_viewportRectY = y;
		m_viewportRectW = w; 
		m_viewportRectH = h;
	}

	void SetAspect(float aspect) { m_aspect = aspect; }

	void SetLens(float fov, float zNear, float zFar)
	{
		m_fov = fov;
		m_nearZ = zNear;
		m_farZ = zFar;
	}

private:
	float m_viewportRectX = 0.0f;
	float m_viewportRectY = 0.0f;
	float m_viewportRectW = 1.0f;
	float m_viewportRectH = 1.0f;

	float m_fov = XM_PIDIV4;
	float m_aspect = 1280.0f / 720.0f;
	float m_nearZ = 0.1f;
	float m_farZ = 1000.0f;
};

