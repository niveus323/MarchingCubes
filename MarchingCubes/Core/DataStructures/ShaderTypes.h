#pragma once
#include <DirectXMath.h>

struct alignas(256) CameraConstants
{
	DirectX::XMFLOAT4X4 viewProjMatrix;
};

struct alignas(256) ObjectConstants
{
	DirectX::XMFLOAT4X4 worldMatrix;
	DirectX::XMFLOAT4 objectColor = { 1.0f, 1.0f, 1.0f, 1.0f };
};

struct alignas(256) PickIDConstants
{
	DirectX::XMFLOAT4 idColor;
};