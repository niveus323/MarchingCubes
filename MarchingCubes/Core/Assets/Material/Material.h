#pragma once
#include "Core/DataStructures/ShaderTypes.h"
using Microsoft::WRL::ComPtr;
using MaterialHandle = uint32_t;

/* [Material]
* - LifeTime : Material Asset Load -> UnLoad 혹은 Material Data Add -> UnLoad
* - OwnerShip : MaterialRegistry
* - Responsibility :
*	- Rendering Setting : GPU에서 읽을 Material 원소(MaterialConstants, 텍스쳐)의 인덱스 관리
*   - Asset Path Caching : 직렬화 시 value(index) -> key(path) 최적화를 위해 에셋 경로 캐싱
*/
struct Material
{
	friend class MaterialRegistry;
public:
	std::string_view GetPSOName() const { return m_psoName; }
	MaterialConstants GetConstants() const { return m_cb; }
	const TextureParams& GetTextureParams() const { return m_cb.baseTextures; }
private:
	std::string m_psoName = "Filled";
	MaterialConstants		m_cb{};
};