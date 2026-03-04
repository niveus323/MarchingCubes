#pragma once
#include "TransformableComponent.h"

class RendererComponent : public TransformableComponent
{
	REFLECT_GENERATED_BODY(RendererComponent)
public:
	virtual void Init() override;
	virtual void Destroy() override;
	virtual void Submit() = 0;
};

