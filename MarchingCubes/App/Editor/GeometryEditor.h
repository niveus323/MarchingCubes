#pragma once
#include "Interface/EditorApp.h"
class GeometryEditor : public EditorApp
{
public:
	GeometryEditor(uint32_t width, uint32_t height, std::wstring name) : EditorApp(width, height, name) {}
	virtual void OnDestroy() override;

protected:
	virtual void InitScene(ID3D12GraphicsCommandList* cmd) override;
	virtual void InitUI() override;
	virtual void UpdateScene(float deltaTime) override;
	virtual void UpdateUI(float deltaTime) override;


private:


};