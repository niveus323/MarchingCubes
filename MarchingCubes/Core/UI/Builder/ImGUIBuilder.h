#pragma once
#include "UIBuilder.h"
#include <imgui.h>

class ImGUIBuilder : public IUIBuilder
{
public:
	// --- 윈도우/패널 관리 ---
	virtual bool BeginPanel(const char* name, bool* pOpen, UI::UI_PanelOption flags = UI::UI_PanelOption::None) override;
	virtual void EndPanel() override;
	
	// --- Table ---
	virtual bool BeginTable(const char* id, int columns) override;
	virtual void EndTable() override;
	virtual bool CollapsingHeader(const char* label, bool defaultOpen = true) override;

	// --- Tab ---
	virtual bool BeginTabBar(const char* id) override;
	virtual void EndTabBar() override;
	virtual bool BeginTabItem(const char* id, bool* pOpen = nullptr) override;
	virtual void EndTabItem() override;
	virtual bool BeginToolBar(const char* id, float size = 30.0f) override;
	virtual void EndToolBar() override;

	// --- Menu ---
	virtual void BeginMainMenuBar() override;
	virtual void EndMainMenuBar() override;
	virtual bool BeginMenuBar() override;
	virtual void EndMenuBar() override;
	virtual bool BeginMenu(const char* id) override;
	virtual void EndMenu() override;
	virtual bool MenuItem(const char* id, const char* shortcutKey = NULL, bool bSelected = false) override;

	// --- Popup ---
	virtual void OpenPopup(const char* id) override;
	virtual bool BeginPopup(const char* id) override;
	virtual void EndPopup() override;
	virtual void CloseCurrentPopup() override;
	virtual bool BeginPopupContextItem() override;

	// --- 기본 컨트롤 ---
	virtual void BeginDisabled(bool disabled = true) override;
	virtual void EndDisabled() override;
	virtual void Label(const char* text) override;
	virtual bool Button(const char* label, const UI::Vector<float, 2>& size) override;
	virtual void InvisibleButton(const char* str_id, const UI::Vector<float, 2>& size) override;
	virtual bool Checkbox(const char* label, bool* v) override;
	virtual void Text(const char* text) override;
	virtual void Text(const std::string& text) override;
	virtual void TextFormatted(const char* fmt, ...) override;
	virtual void TextColored(const UI::Color& color, const char* fmt, ...) override;
	virtual void Image(void* textureHandle, const UI::Vector<float, 2>& size) override;
	virtual bool Selectable(const char* label) override;

	// --- 입력 컨트롤 ---
	virtual bool InputText(const char* label, std::string& text);
	virtual bool InputEnum(const char* label, int* currentValue, const std::vector<std::string>& names, const std::vector<int>& values) override;
	virtual bool BeginDragDropSource() override;
	virtual void SetDragDropPayload(const char* type, const void* data, size_t size) override;
	virtual void EndDragDropSource() override;
	virtual bool BeginDragDropTarget() override;
	virtual const void* AcceptDragDropPayload(const char* type) override;
	virtual void EndDragDropTarget() override;

	// --- 테이블 컨트롤 ---
	virtual void TableHeadersRow() override;
	virtual void TableNextRow() override;
	virtual void TableNextColumn() override;
	virtual void TableSetColumnIndex(int index) override;
	virtual void TableSetupColumn(const char* id) override;
	virtual void PropertyText(const char* label, const char* value) override;
	virtual bool PropertyInputText(const char* label, std::string& text) override;
	virtual bool PropertyEnum(const char* label, int* currentValue, const std::vector<std::string>& names, const std::vector<int>& values) override;

	// --- 레이아웃 헬퍼 ---
	virtual void Separator() override;
	virtual void SameLine(float offset, float spacing) override;
	virtual void Indent(float width) override;
	virtual void Unindent(float width) override;
	virtual void AlignNextItem(const UI::Vector<float, 2> size, UI::UI_AlignmentX alignX = UI::UI_AlignmentX::Align_Left, UI::UI_AlignmentY alignY = UI::UI_AlignmentY::Align_Top) override;
	virtual void SetNextWindowAligned(UI::Vector<float, 2> pos, UI::UI_AlignmentX alignX = UI::UI_AlignmentX::Align_Left, UI::UI_AlignmentY alignY = UI::UI_AlignmentY::Align_Top) override;
	virtual void PushStyle_Padding(const UI::Vector<float, 2>& padding) override;
	virtual void PushStyle_Button(const UI::Color& defaultColor, const UI::Color& hoverColor, const UI::Color& activeColor) override;
	virtual void PopStyle_Var(int count = 1) override;
	virtual void PopStyle_Color(int count = 1) override;
	virtual void SetNextItemWidth(float width) override;
	virtual UI::Vector<float, 2> CalcTextSize(const char* text) override;

	// --- ID 관리 ---
	virtual void PushID(const char* str_id) override;
	virtual void PushID(const void* ptr_id) override;
	virtual void PopID() override;

	// --- 상태 체크 ---
	virtual bool IsItemClicked() override;
	virtual bool IsItemDoubleClicked() override;
	virtual bool IsItemHovered() override;
	virtual bool IsItemActive() override;
	virtual bool IsMouseHoveringRect(const UI::Vector<float, 2>& pMin, const UI::Vector<float, 2>& pMax, bool clip = true) override;
	virtual bool IsAnyItemHovered() override;
	virtual bool IsItemDeactivated() override;
	virtual bool IsWindowFocused() override;
	virtual bool IsWindowHovered() override;
	virtual bool IsKeyPressed_F12() override;
	virtual bool IsKeyPressed_Delete() override;
	virtual void SetKeyboardFocus() override;
	virtual bool IsMouseClicked(int button) override;
	virtual bool IsMouseReleased(int button) override;
	virtual bool IsMouseDragging(int button) override;
	virtual UI::Vector<float, 2> GetMousePos() override;
	virtual void SetCursorScreenPos(const UI::Vector<float, 2>& pos) override;

	// --- Search ---
	virtual bool SearchBar(const char* hint, std::string& text) override;

	// --- Selectable Combo Box ---
	virtual void SelectableComboBox(const char* label, const std::vector<std::string>& items, int& selectedItem, UI::Vector<float, 2> size = { 0.0f,0.0f }) override;

	// --- Dual List Box ---
	virtual bool DualListBox(const char* label, std::vector<int>& availableItems, std::vector<int>& basketItems, std::function<std::string(int)> getItemNameFn) override;

	// --- Hierarchy Tree ---
	virtual bool BeginTreeNode(const char* label, bool isLeaf, bool isSelected) override;
	virtual void EndTreeNode() override;

	// --- Graphs ---
	virtual void PlotLines(const char* label, const float* values, int count) override;
	virtual float GetAvailableWidth() override;
	virtual UI::Vector<float, 2> GetRegionAvailable() override;
	virtual UI::Vector<float, 2> GetWindowContentMin() override;
	virtual UI::Vector<float, 2> GetWindowContentMax() override;
	virtual UI::Vector<float, 2> GetCursorScreenPos() override;
	virtual UI::Vector<float, 2> GetWindowPos()override;
	virtual void Dummy(const UI::Vector<float, 2>& size) override;
	virtual void DrawRect(const UI::Vector<float, 2>& p0, const UI::Vector<float, 2>& p1, const UI::Color& color) override;
	virtual void DrawRectFilled(const UI::Vector<float, 2>& p0, const UI::Vector<float, 2>& p1, const UI::Color& color) override;

	// --- ToopTip ---
	virtual void BeginTooltip() override;
	virtual void EndTooltip() override;

	// --- Window/Overlay ---
	virtual bool BeginOverlay(const char* name, const UI::Vector<float, 2>& pos, const UI::Vector<float, 2>& size, float alpha = 0.0f, UI::UI_PanelOption flags = UI::UI_PanelOption::None, UI::UI_AlignmentX alignX = UI::UI_AlignmentX::Align_Left, UI::UI_AlignmentY alignY = UI::UI_AlignmentY::Align_Top) override;
	virtual void EndOverlay() override;
	virtual UI::Vector<float, 2> GetMainViewportPos() override;
	virtual UI::Vector<float, 2> GetMainViewportSize() override;
	virtual void DockSpaceOverViewport(const char* id, const void* viewport = nullptr) override;
	virtual void SetNextWindowDocking(const char* dockspaceId, UI::UI_Condition cond = UI::UI_Condition::FirstUseEver, UI::UI_DockingOption option = UI::UI_DockingOption::None) override;

	// --- Primitive ---
	virtual void DrawLine(const UI::Vector<float, 2>& p1, const UI::Vector<float, 2>& p2, const UI::Color& color, float thickness = 1.0f) override;
	virtual void DrawCircleFilled(const UI::Vector<float, 2>& center, float radius, const UI::Color& color) override;
	virtual void DrawTextAt(const UI::Vector<float, 2>& pos, const UI::Color& color, const char* text) override;

	// --- Gizmo ---
	virtual bool IsGizmoHovered() override;
	_Success_(return)
	virtual bool DrawTransformGizmo(const DirectX::XMFLOAT4X4& view, const DirectX::XMFLOAT4X4& proj, UI::EGizmoOperation op, UI::EGizmoMode mode, _Inout_ DirectX::XMFLOAT4X4& world, _Out_ DirectX::XMFLOAT3& translation, _Out_ DirectX::XMFLOAT3& rotation, _Out_ DirectX::XMFLOAT3& scale, const float* snap, float gizmoSize = 0.1f) override;
private:
	virtual bool InputInternal(const char* label, UI::UI_DataType type, void* pValue) override;
	virtual bool DragInternal(const char* label, UI::UI_DataType type, void* pValue, float speed) override;
	virtual bool PropertyInternal(const char* label, UI::UI_DataType type, void* pValue, float speed) override;
	std::string DrawPropertyLabel(const char* label);

	// --- Flags ---
	ImGuiWindowFlags GetWindowFlags(UI::UI_PanelOption flags);
};

