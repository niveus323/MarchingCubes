#pragma once
#include "UIBuilder.h"
#include <imgui.h>

class ImGUIBuilder : public IUIBuilder
{
public:
	// --- 윈도우/패널 관리 ---
	virtual bool BeginPanel(const char* name, bool* pOpen) override;
	virtual void EndPanel() override;

	virtual bool BeginTable(const char* id, int columns) override;
	virtual void EndTable() override;
	virtual void TableNextRow() override;
	virtual void TableNextColumn() override;
	virtual bool BeginCollapsingHeader(const char* label, bool defaultOpen = true) override;

	virtual bool BeginTabBar(const char* id) override;
	virtual void EndTabBar() override;
	virtual bool BeginTabItem(const char* id, bool* pOpen = nullptr) override;
	virtual void EndTabItem() override;

	// --- 기본 컨트롤 ---
	virtual void Label(const char* text) override;
	virtual bool Button(const char* label, const UI::Vector2& size) override;
	virtual bool Checkbox(const char* label, bool* v) override;
	virtual void Text(const char* text) override;
	virtual void Text(const std::string& text) override;
	virtual void Image(void* textureHandle, const UI::Vector2& size) override;

	// --- 입력 컨트롤 ---
	virtual bool InputInt(const char* label, int* v) override;
	virtual bool DragFloat(const char* label, float* v, float speed = 1.0f) override;
	virtual bool DragFloat3(const char* label, float* v, float speed = 1.0f) override;
	virtual bool EditColor3(const char* label, float* v) override;
	virtual bool InputText(const char* label, std::string& text);
	virtual bool InputEnum(const char* label, int* currentValue, const std::vector<std::string>& names, const std::vector<int>& values) override;

	// --- 테이블 컨트롤 ---
	virtual bool PropertyBool(const char* label, bool* v) override;
	virtual bool PropertyInt(const char* label, int* v) override;
	virtual bool PropertyFloat(const char* label, float* v, float speed = 1.0f) override;
	virtual bool PropertyFloat3(const char* label, float* v, float speed = 1.0f) override;
	virtual bool PropertyColor(const char* label, float* v) override;
	virtual void PropertyText(const char* label, const char* value) override;
	virtual bool PropertyInputText(const char* label, std::string& text) override;
	virtual bool PropertyEnum(const char* label, int* currentValue, const std::vector<std::string>& names, const std::vector<int>& values) override;

	// --- 레이아웃 헬퍼 ---
	virtual void Separator() override;
	virtual void SameLine(float offset, float spacing) override;

	// --- ID 관리 ---
	virtual void PushID(const char* str_id) override;
	virtual void PushID(const void* ptr_id) override;
	virtual void PopID() override;

	// --- 상태 체크 ---
	virtual bool IsItemClicked() override;
	virtual bool IsItemHovered() override;
	virtual bool IsItemActive() override;
	virtual bool IsMouseHoveringRect(const UI::Vector2& pMin, const UI::Vector2& pMax, bool clip = true) override;
	virtual bool IsAnyItemHovered() override;
	virtual bool IsItemDeactivated() override;
	virtual bool IsWindowFocused() override;
	virtual bool IsKeyPressed_F12() override;
	virtual void SetKeyboardFocus() override;
	virtual bool IsMouseClicked(int button) override;
	virtual bool IsMouseReleased(int button) override;
	virtual bool IsMouseDragging(int button) override;
	virtual UI::Vector2 GetMousePos() override;
	virtual void SetCursorScreenPos(const UI::Vector2& pos) override;

	// --- 검색 창 ---
	virtual bool SearchBar(const char* hint, std::string& text) override;

	// --- Hierarchy Tree ---
	virtual bool BeginTreeNode(const char* label, bool isLeaf, bool isSelected) override;
	virtual void EndTreeNode() override;

	// --- 그래프 ---
	virtual void PlotLines(const char* label, const float* values, int count) override;
	virtual float GetAvailableWidth() override;
	virtual UI::Vector2 GetCursorScreenPos() override;
	virtual void Dummy(const UI::Vector2& size) override;
	virtual void DrawRect(const UI::Vector2& p0, const UI::Vector2& p1, const UI::Color& color) override;
	virtual void DrawRectFilled(const UI::Vector2& p0, const UI::Vector2& p1, const UI::Color& color) override;

	// --- ToopTip ---
	virtual void BeginTooltip() override;
	virtual void EndTooltip() override;

	// --- Window/Overlay ---
	virtual bool BeginOverlay(const char* name, const UI::Vector2& pos, const UI::Vector2& size) override;
	virtual void EndOverlay() override;
	virtual UI::Vector2 GetMainViewportPos() override;

	// --- Primitive ---
	virtual void DrawLine(const UI::Vector2& p1, const UI::Vector2& p2, const UI::Color& color, float thickness = 1.0f) override;
	virtual void DrawCircleFilled(const UI::Vector2& center, float radius, const UI::Color& color) override;
	virtual void DrawTextAt(const UI::Vector2& pos, const UI::Color& color, const char* text) override;
	virtual UI::Vector2 CalcTextSize(const char* text) override;
	virtual void InvisibleButton(const char* str_id, const UI::Vector2& size) override;

private:
	std::string DrawPropertyLabel(const char* label);
	// UI::Color(r,g,b,a float) -> ImU32 변환
	ImU32 ColorToImU32(const UI::Color& color);
};

