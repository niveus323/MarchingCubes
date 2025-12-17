#pragma once
#include "Core/Scene/Component/RendererComponent.h"
#include "Core/Geometry/Mesh/MeshChunkRenderer.h"

class TerrainRendererComponent : public RendererComponent
{
public:
	TerrainRendererComponent(SceneObject* owner) : RendererComponent(owner) {}
	virtual ~TerrainRendererComponent() = default;

	void SetChunkRenderer(MeshChunkRenderer* renderer) { m_chunkRenderer = renderer; }
	void SetMaterial(uint32_t matIndex) { m_materialInstance.index = matIndex; }
	void SetPSO(std::string_view psoName) { m_materialInstance.psoName = psoName; }
	virtual void Submit() override;

private:
	MeshChunkRenderer* m_chunkRenderer = nullptr;
	MaterialInstance m_materialInstance;
};

