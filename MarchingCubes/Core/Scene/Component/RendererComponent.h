#pragma once
#include "TransformableComponent.h"
#include "Core/DataStructures/Data.h"

class RendererComponent : public TransformableComponent
{
	REFLECT_GENERATED_BODY(RendererComponent)
public:
	virtual void Init() override;
	virtual void Destroy() override;
	virtual void Submit() = 0;

	void AddOverlayPass(const std::string& name, const std::string& psoName, std::vector<ShaderBinding> extraBindings, bool bInitialState = false);
	void SetOverlayPassActive(const std::string& name, bool bActive);
	void RemoveOverlayPass(const std::string& name);
protected:
	struct OverlayPass
	{
		std::string name;
		std::string psoName;
		bool bActive = false;
		std::vector<ShaderBinding> resourceBindings;
	};
	std::vector<OverlayPass> m_overlayPasses;
};

