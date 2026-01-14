#pragma once
#include "Core/UI/Builder/UIBuilder.h"

class IEditorPanel
{
public:
	virtual ~IEditorPanel() = default;
	virtual void OnUpdate() {};
	virtual void OnRenderUI(IUIBuilder* ui) = 0;
};

