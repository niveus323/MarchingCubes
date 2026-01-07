#pragma once
#include <format>

namespace UI
{
    struct Vector2 { float x, y; };
    struct Vector3 { float x, y, z; };
    struct Color { float r, g, b, a; };

    enum class ControlFlags { None = 0, ReadOnly = 1 << 0 };
}

class IUIBuilder
{
public:
    virtual ~IUIBuilder() = default;

    // --- 윈도우/패널 관리 ---
    virtual bool BeginPanel(const char* name, bool* pOpen = nullptr) = 0;
    virtual void EndPanel() = 0;

    virtual bool BeginTable(const char* id, int columns) = 0;
    virtual void EndTable() = 0;
    virtual void TableNextRow() = 0;
    virtual void TableNextColumn() = 0;
    virtual bool BeginCollapsingHeader(const char* label, bool defaultOpen = true) = 0;

    // --- 기본 컨트롤 ---
    virtual void Label(const char* text) = 0;
    virtual bool Button(const char* label, const UI::Vector2& size = { 0,0 }) = 0;
    virtual bool Checkbox(const char* label, bool* v) = 0;
    virtual void Text(const char* text) = 0;
    virtual void Text(const std::string& text) = 0;
    
    // --- 입력 컨트롤 ---
    virtual bool InputInt(const char* label, int* v) = 0;
    virtual bool DragFloat(const char* label, float* v, float speed = 1.0f) = 0;
    virtual bool DragFloat3(const char* label, float* v, float speed = 1.0f) = 0;
    virtual bool EditColor3(const char* label, float* v) = 0;
    virtual bool InputText(const char* label, std::string& text) = 0;
    virtual bool InputEnum(const char* label, int* currentValue, const std::vector<std::string>& names, const std::vector<int>& values) = 0;

    // --- 테이블 컨트롤 ---
    virtual bool PropertyBool(const char* label, bool* v) = 0;
    virtual bool PropertyInt(const char* label, int* v) = 0;
    virtual bool PropertyFloat(const char* label, float* v, float speed = 1.0f) = 0;
    virtual bool PropertyFloat3(const char* label, float* v, float speed = 1.0f) = 0;
    virtual bool PropertyColor(const char* label, float* v) = 0;
    virtual void PropertyText(const char* label, const char* value) = 0;
    virtual bool PropertyInputText(const char* label, std::string& text) = 0;
    virtual bool PropertyEnum(const char* label, int* currentValue, const std::vector<std::string>& names, const std::vector<int>& values) = 0;

    // --- 레이아웃 헬퍼 ---
    virtual void Separator() = 0;
    virtual void SameLine(float offset_from_start_x = 0.0f, float spacing = -1.0f) = 0;

    // --- ID 관리 ---
    virtual void PushID(const char* str_id) = 0;
    virtual void PushID(const void* ptr_id) = 0;
    virtual void PopID() = 0;

    // --- 상태 체크 ---
    virtual bool IsItemClicked() = 0;
    virtual bool IsItemHovered() = 0;
    virtual bool IsMouseHoveringRect(const UI::Vector2& pMin, const UI::Vector2& pMax, bool clip = true) = 0;
    virtual bool IsAnyItemHovered() = 0;
    virtual bool IsItemDeactivated() = 0;
    virtual bool IsWindowFocused() = 0;
    virtual bool IsKeyPressed_F12() = 0;

    // --- 포커스 상태 ---
    virtual void SetKeyboardFocus() = 0;

    // --- 검색 창 ---
    virtual bool SearchBar(const char* hint, std::string& text) = 0;

    // --- Hierarchy Tree ---
    virtual bool BeginTreeNode(const char* label, bool isLeaf, bool isSelected) = 0;
    virtual void EndTreeNode() = 0;

    // --- 그래프 ---
    virtual void PlotLines(const char* label, const float* values, int count) = 0;
    virtual float GetAvailableWidth() = 0;
    virtual UI::Vector2 GetCursorScreenPos() = 0;
    virtual void Dummy(const UI::Vector2& size) = 0;
    virtual void DrawRect(const UI::Vector2& p0, const UI::Vector2& p1, const UI::Color& color) = 0;
    virtual void DrawRectFilled(const UI::Vector2& p0, const UI::Vector2& p1, const UI::Color& color) = 0;

    // --- ToopTip ---
    virtual void BeginTooltip() = 0;
    virtual void EndTooltip() = 0;

    // --- Window/Overlay ---
    virtual bool BeginOverlay(const char* name, const UI::Vector2& pos, const UI::Vector2& size) = 0;
    virtual void EndOverlay() = 0;
    virtual UI::Vector2 GetMainViewportPos() = 0;

    // --- Primitive ---
    virtual void DrawLine(const UI::Vector2& p1, const UI::Vector2& p2, const UI::Color& color, float thickness = 1.0f) = 0;
    virtual void DrawCircleFilled(const UI::Vector2& center, float radius, const UI::Color& color) = 0;
    virtual void DrawTextAt(const UI::Vector2& pos, const UI::Color& color, const char* text) = 0;
    virtual UI::Vector2 CalcTextSize(const char* text) = 0;

};

