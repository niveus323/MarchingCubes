#pragma once
#include "Component.h"

class TransformComponent;

class TransformableComponent : public Component
{
	REFLECT_GENERATED_BODY(TransformableComponent)
public:
	virtual void Init() override;

	TransformComponent* GetTransformComp() const;

protected:
	TransformComponent* m_transformCache = nullptr;
};

