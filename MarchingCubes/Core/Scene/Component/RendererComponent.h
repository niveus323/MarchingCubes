#pragma once
#include "Core/Scene/Component/Component.h"

class SceneObject;

class RendererComponent : public Component
{
public:
	RendererComponent(SceneObject* owner);
	~RendererComponent() = default;

	virtual void Init() override;
	virtual void Destroy() override;
	virtual void Submit() = 0;
};

