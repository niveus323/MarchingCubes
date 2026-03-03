#pragma once
#include "Core/DataStructures/ShaderTypes.h"
#include "Core/Assets/TextureAsset.h"
#include <variant>

/* [MaterialAsset]
* - LifeTime : Material Asset Load -> UnLoad
* - OwnerShip : MaterialRegistry
* - Responsibility :
*   - Serialized Data : 데이터 파일 경로, PSO & 상수 데이터 등 직렬화된 데이터를 저장
*   - Texture Asset Caching : Material 생성에 필요한 텍스쳐 에셋을 참조
*/

class MaterialAsset
{
    friend class ResourceManager;
public:
    std::string_view GetPath() const { return m_assetPath; }
    std::string_view GetPSO() const { return m_psoName; }
    const MaterialConstants& GetConstants() const { return m_constants; }
    std::shared_ptr<TextureAsset> GetDiffuse() const { return m_diffuse; }
    std::shared_ptr<TextureAsset> GetNormal() const { return m_normal; }
    std::shared_ptr<TextureAsset> GetARM() const { return m_arm; }
    std::shared_ptr<TextureAsset> GetDisplacement() const { return m_displacement; }
    std::shared_ptr<TextureAsset> GetRoughness() const { return m_roughness; }
    std::shared_ptr<TextureAsset> GetEmissive() const { return m_emissive; }
    std::shared_ptr<TextureAsset> GetMetallic() const { return m_metallic; }

private:
    std::string m_assetPath = "";
    std::string m_psoName = "Filled";
    MaterialConstants m_constants{};

    std::shared_ptr<TextureAsset> m_diffuse;
    std::shared_ptr<TextureAsset> m_normal;
    std::shared_ptr<TextureAsset> m_arm;
    std::shared_ptr<TextureAsset> m_displacement;
    std::shared_ptr<TextureAsset> m_roughness;
    std::shared_ptr<TextureAsset> m_emissive;
    std::shared_ptr<TextureAsset> m_metallic;
};

using MaterialParameterValue = std::variant<
    float,
    int,
    DirectX::XMFLOAT3,
    DirectX::XMFLOAT4,
    std::shared_ptr<TextureAsset> // 텍스처 오버라이드용
>;
struct MaterialInstance
{
    std::shared_ptr<MaterialAsset> m_material;
    std::string m_psoName = "";
    std::unordered_map<std::string, MaterialParameterValue> m_overrides;
    bool m_bDirty = false;

    template<typename T>
    void SetParameter(const std::string& name, const T& value)
    {
        m_overrides[name] = value;
        m_bDirty = true;
    }
    std::string_view GetPSO()
    {
        if (m_psoName.empty()) return m_material->GetPSO();
        return m_psoName;
    }
};