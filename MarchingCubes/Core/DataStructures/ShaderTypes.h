#pragma once
#include <DirectXMath.h>
using namespace DirectX;

// alignment
static constexpr UINT CB_ALIGN = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;

struct alignas(16) CameraConstants
{
	XMFLOAT4X4 viewProjMatrix;
	XMFLOAT3 cameraPosition;
	float _padding;
};

struct alignas(16) ObjectConstants
{
	XMFLOAT4X4 worldMatrix;
	XMFLOAT4X4 worldInvMatrix;
};

enum class EShadingModel : uint32_t
{
	DefaultLit = 0,
	Dielectric,
	Translucent,
	_Count
};

struct alignas(16) MaterialConstants
{
	XMFLOAT3 albedo;    // albedo 색상 (RGB)
	float metallic;				 // [0,1]

	float roughness;			 // [0,1]
	float specularStrength;		 // 비금속 반사 강도 [0,0.08]
	float ao;					 // AmbientOcclusion
	float ior;					 // Dielectric 모델 전용 굴절률

	EShadingModel shadingModel;	 // 반사 모델링 타입
	float opacity = 1.0f;
	float _padding[2];
};

enum class ELightType : uint32_t
{
	Directional = 0,
	Point = 1,
	Spot = 2,
	_Count
};

/*
* LightType 별 필요한 데이터
* 기본 : type, radiance
* Directional : dir
* Point : pos, range
* Spot : pos, range, spotDir, spotInnerCos
*/
struct alignas(16) Light
{
	ELightType type;
	uint32_t _padding0[3];
	XMFLOAT3 radiance;
	float _padding1;
	XMFLOAT3 dirOrPos;
	float rangeOrPadding;
	XMFLOAT3 spotDir;
	float spotInnerCos;
};
static_assert(sizeof(Light) == 64 && "Light layout mismatch!!!!");

struct alignas(16) LightConstants
{
	uint32_t lightCounts;
	uint32_t _padding[3];
	Light lights[1]; //가변 크기로 할당하여 사용
};

struct alignas(16) LightConstantsHeader
{
	uint32_t lightCounts;
	uint32_t _padding[3];
};