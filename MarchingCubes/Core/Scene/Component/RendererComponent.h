#pragma once
#include "Core/Scene/Component/Component.h"

class RendererComponent : public Component
{
public:
	RendererComponent(SceneObject* owner);
	~RendererComponent() = default;

	virtual void Submit() = 0;
};

