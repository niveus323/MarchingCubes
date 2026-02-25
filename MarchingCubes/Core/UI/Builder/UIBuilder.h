#pragma once
#include "Core/Utils/EnumBitmask.h"
#include <format>
#include <concepts>
#include <cstdint>
#include <functional>


namespace UI
{
    template<typename T, int N>
    struct Vector 
    {
        T data[N];

        T& operator[](int i) { return Data[i]; }
        const T& operator[](int i) const { return Data[i]; }
        T* Ptr() { return data; }
    };

    template<typename T>
    struct Vector<T, 2> {
        union {
            T Data[2];           
            struct { T x, y; };  
            struct { T u, v; };  
        };

        Vector(T _x, T _y) : x(_x), y(_y) {}
        Vector() : x(0), y(0) {}

        T& operator[](int i) { return Data[i]; }
        T* Ptr() { return Data; }
    };

    template<typename T>
    struct Vector<T, 3> {
        union {
            T Data[3];
            struct { T x, y, z; }; 
            struct { T r, g, b; }; 
        };

        Vector(T _x, T _y, T _z) : x(_x), y(_y), z(_z) {}
        Vector() : x(0), y(0), z(0) {}

        T& operator[](int i) { return Data[i]; }
        T* Ptr() { return Data; }
    };

    template<typename T>
    struct Vector<T, 4> {
        union {
            T Data[4];
            struct { T x, y, z, w; };
            struct { T r, g, b, a; };
        };

        Vector(T _x, T _y, T _z, T _w) : x(_x), y(_y), z(_z),w(_w) {}
        Vector() : x(0), y(0), z(0), w(0) {}

        T& operator[](int i) { return Data[i]; }
        T* Ptr() { return Data; }
    };

    struct Color : public Vector<float, 4>
    {
        // 생성자 상속
        using Vector<float, 4>::Vector;

        Color() : Vector<float, 4>(1.0f, 1.0f, 1.0f, 1.0f) {}
        Color(float _r, float _g, float _b, float _a) : Vector<float, 4>(_r, _g, _b, _a) {}
        Color(unsigned int hexValue) : Vector<float, 4>(FromRGBA(hexValue)) {}

        // HSV 변환 생성 (H: 0~1, S: 0~1, V: 0~1)
        // 생성자와의 모호함을 피하기 위해 static 함수로 선언
        static Color FromHSV(float h, float s, float v, float a = 1.0f)
        {
            float r = 0.0f, g = 0.0f, b = 0.0f;
            if (s == 0.0f)
            {
                // Saturation이 0이면 회색 (Grayscale)
                r = g = b = v;
            }
            else
            {
                float h_wrap = fmodf(h, 1.0f) * 6.0f;
                int i = static_cast<int>(h_wrap);
                float f = h_wrap - i;
                float p = v * (1.0f - s);
                float q = v * (1.0f - s * f);
                float t = v * (1.0f - s * (1.0f - f));

                switch (i % 6)
                {
                    case 0: 
                        r = v; g = t; b = p;
                    break;
                    case 1: 
                        r = q; g = v; b = p;
                    break;
                    case 2:
                        r = p; g = v; b = t;
                    break;
                    case 3:
                        r = p; g = q; b = v;
                    break;
                    case 4:
                        r = t; g = p; b = v;
                    break;
                    case 5:
                        r = v; g = p; b = q;
                    break;
                }
            }
            return Color(r, g, b, a);
        }

        // Hex 포맷이 0xRRGGBBAA 인 경우를 위한 명시적 함수
        static Color FromRGBA(unsigned int hex)
        {
            return Color(
                ((hex >> 16) & 0xFF) / 255.0f,
                ((hex >> 8) & 0xFF) / 255.0f,
                ((hex) & 0xFF) / 255.0f, 
                ((hex >> 24) & 0xFF) / 255.0f
            );
        }

        // -> UINT 유틸리티
        unsigned int ToUInt() const {
            auto c = [](float v) { return static_cast<unsigned int>(std::clamp(v, 0.f, 1.f) * 255.f); };
            return (c(a) << 24) | (c(r) << 16) | (c(g) << 8) | c(b);
        }
    };

    // NOTE : Vector형은 순서 변경 절대 X
    enum class UI_DataType 
    {
        Bool, 
        Int, 
        Int2, 
        Int3,
        UInt,
        UInt2,
        UInt3,
        Float, 
        Float2, 
        Float3,
        Float4,
        Color,
        Unknown
    };
    
    template<typename T> struct BaseUIEnum { static constexpr UI_DataType value = UI_DataType::Unknown; };
    template<> struct BaseUIEnum<bool> { static constexpr UI_DataType value = UI_DataType::Bool; };
    template<> struct BaseUIEnum<int> { static constexpr UI_DataType value = UI_DataType::Int; };
    template<> struct BaseUIEnum<unsigned int> { static constexpr UI_DataType value = UI_DataType::UInt; };
    template<> struct BaseUIEnum<float> { static constexpr UI_DataType value = UI_DataType::Float; };

    // Duck-typing : 실수형 벡터 추론
    template<typename T>
    using MemberXType = std::remove_cvref_t<decltype(std::declval<T>().x)>;

    // Int2 ~ Int 3
    template<typename T>
    concept IntVectorLike = std::is_standard_layout_v<T> && requires(T a) { { a.x }; } && std::is_same_v<int, MemberXType<T>> 
        && (sizeof(T) % sizeof(int) == 0) &&(sizeof(T) / sizeof(int) >= 2) && (sizeof(T) / sizeof(int) <= 3);
    
    // UINT2 ~ UINT3
    template<typename T>
    concept UIntVectorLike = std::is_standard_layout_v<T> && requires(T a) { a.x; }&& std::is_same_v<unsigned int, MemberXType<T>> &&
        (sizeof(T) % sizeof(unsigned int) == 0) && (sizeof(T) / sizeof(unsigned int) >= 2) && (sizeof(T) / sizeof(unsigned int) <= 3);

    // Float2 ~ Float4
    template<typename T>
    concept FloatVectorLike = std::is_standard_layout_v<T> && requires(T a) { { a.x }; } && std::is_same_v<float, MemberXType<T>> 
        && (sizeof(T) % sizeof(float) == 0) && (sizeof(T) / sizeof(float) >= 2) && (sizeof(T) / sizeof(float) <= 4);

    template<typename T>
    struct UITypeTraits 
    {
        // 기본 타입은 BaseUIEnum 그대로 사용
        static constexpr UI_DataType value = BaseUIEnum<T>::value;
    };

    template<IntVectorLike T>
    struct UITypeTraits<T>
    {
        static constexpr int N = sizeof(T) / sizeof(float);
        static constexpr UI_DataType value = static_cast<UI_DataType>(static_cast<int>(BaseUIEnum<int>::value) + (N - 1));
    };

    template<UIntVectorLike T>
    struct UITypeTraits<T> {
        static constexpr int N = sizeof(T) / sizeof(unsigned int);
        static constexpr UI_DataType value = static_cast<UI_DataType>(static_cast<int>(BaseUIEnum<unsigned int>::value) + (N - 1));
    };

    template<FloatVectorLike T>
    struct UITypeTraits<T> 
    {
        static constexpr int N = sizeof(T) / sizeof(float);
        static constexpr UI_DataType value = static_cast<UI_DataType>(static_cast<int>(BaseUIEnum<float>::value) + (N - 1));
    };
    
    template<>
    struct UITypeTraits<Color> 
    {
        static constexpr UI_DataType value = UI_DataType::Color;
    };

    template<typename T>
    constexpr UI_DataType GetUIType() 
    {
        return UITypeTraits<T>::value;
    }

    inline int GetComponentCount(UI_DataType type)
    {
        switch (type) {
            case UI_DataType::Int2: 
            case UI_DataType::Float2: 
            case UI_DataType::UInt2:
                return 2;
            case UI_DataType::Int3: 
            case UI_DataType::Float3: 
            case UI_DataType::UInt3:
                return 3;
            case UI_DataType::Float4:
            case UI_DataType::Color: 
                return 4;
        }
        return 1; // Int, Float, Bool 등
    }

    enum class UI_Alignment
    {
        AlignLeft,
        AlignCenter,
        AlignRight
    };

    enum class UI_PanelOption : uint32_t
    {
        None = 0,
        MenuBar = 1 << 0,
        NoDocking = 1 << 1,
        NoMove = 1 << 2,
        NoInput = 1 << 3,
        NoScrollBar = 1 << 4
    };
}
ENABLE_BITMASK(UI::UI_PanelOption);

/*
NOTE : UI 빌더 규칙
    1. 위치, 크기 등 제어 가능한 모든 값들은 float 단위로 한다
    2. 값을 입력 받는 함수는 타입 추론(UI_DataType을 활용)하여 사용한다
    3. Color는 (R,G,B,A) 4 원소로 통일하여 사용한다
*/
class IUIBuilder
{
public:
    virtual ~IUIBuilder() = default;

    // --- 윈도우/패널 관리 ---
    virtual bool BeginPanel(const char* name, bool* pOpen = nullptr, UI::UI_PanelOption flags = UI::UI_PanelOption::None) = 0;
    virtual void EndPanel() = 0;

    virtual bool BeginTable(const char* id, int columns) = 0;
    virtual void EndTable() = 0;
    virtual bool BeginCollapsingHeader(const char* label, bool defaultOpen = true) = 0;

    virtual bool BeginTabBar(const char* id) = 0;
    virtual void EndTabBar() = 0;
    virtual bool BeginTabItem(const char* id, bool* pOpen = nullptr) = 0;
    virtual void EndTabItem() = 0;

    virtual void BeginMainMenuBar() = 0;
    virtual void EndMainMenuBar() = 0;
    virtual bool BeginMenuBar() = 0;
    virtual void EndMenuBar() = 0;
    virtual bool BeginMenu(const char* id) = 0;
    virtual void EndMenu() = 0;
    virtual bool MenuItem(const char* id, const char* shortcutKey = NULL, bool bSelected = false) = 0;

    virtual void BeginDisabled(bool disabled = true) = 0; // NOTE : ScopedDisable 사용
    virtual void EndDisabled() = 0;

    // --- 기본 컨트롤 ---
    virtual void Label(const char* text) = 0;
    virtual bool Button(const char* label, const UI::Vector<float, 2>& size = { 0,0 }) = 0;
    virtual bool Checkbox(const char* label, bool* v) = 0;
    virtual void Text(const char* text) = 0;
    virtual void Text(const std::string& text) = 0;
    virtual void TextFormatted(const char* fmt, ...) = 0;
    virtual void TextColored(const UI::Color& color, const char* fmt, ...) = 0;
    virtual void Image(void* textureHandle, const UI::Vector<float, 2>& size) = 0;
    
    // --- 입력 컨트롤 ---
    template<typename T>
    bool Input(const char* label, T* v)
    {
        UI::UI_DataType type = UI::GetUIType<T>();
        return InputInternal(label, type, static_cast<void*>(v));
    }
    template<typename T>
    bool Drag(const char* label, T* v, float speed = 1.0f)
    {
        UI::UI_DataType type = UI::GetUIType<T>();
        return DragInternal(label, type, static_cast<void*>(v), speed);
    }
    virtual bool InputText(const char* label, std::string& text) = 0;
    virtual bool InputEnum(const char* label, int* currentValue, const std::vector<std::string>& names, const std::vector<int>& values) = 0;

    // --- 테이블 컨트롤 ---
    virtual void TableHeadersRow() = 0;
    virtual void TableNextRow() = 0;
    virtual void TableNextColumn() = 0;
    virtual void TableSetColumnIndex(int index) = 0;
    virtual void TableSetupColumn(const char* id) = 0;
    template<typename T>
    bool Property(const char* label, T* v, float speed = 1.0f)
    {
        UI::UI_DataType type = UI::GetUIType<T>();
        return PropertyInternal(label, type, static_cast<void*>(v), speed);
    }
    virtual void PropertyText(const char* label, const char* value) = 0;
    virtual bool PropertyInputText(const char* label, std::string& text) = 0;
    virtual bool PropertyEnum(const char* label, int* currentValue, const std::vector<std::string>& names, const std::vector<int>& values) = 0;

    // --- 레이아웃 헬퍼 ---
    virtual void Separator() = 0;
    virtual void SameLine(float offset_from_start_x = 0.0f, float spacing = -1.0f) = 0;
    virtual void Indent(float width = 0) = 0;
    virtual void Unindent(float width = 0) = 0;
    virtual void AlignNextItem(UI::UI_Alignment align, float itemWidth = 0.0f) = 0;

    // --- ID 관리 ---
    virtual void PushID(const char* str_id) = 0;
    virtual void PushID(const void* ptr_id) = 0;
    virtual void PopID() = 0;

    // --- 상태 체크 ---
    virtual bool IsItemClicked() = 0;
    virtual bool IsItemHovered() = 0;
    virtual bool IsItemActive() = 0;
    virtual bool IsMouseHoveringRect(const UI::Vector<float, 2>& pMin, const UI::Vector<float,2>& pMax, bool clip = true) = 0;
    virtual bool IsAnyItemHovered() = 0;
    virtual bool IsItemDeactivated() = 0;
    virtual bool IsWindowFocused() = 0;
    virtual bool IsWindowHovered() = 0;
    virtual bool IsKeyPressed_F12() = 0;
    virtual void SetKeyboardFocus() = 0;
    virtual bool IsMouseClicked(int button) = 0;
    virtual bool IsMouseReleased(int button) = 0;
    virtual bool IsMouseDragging(int button) = 0;
    virtual UI::Vector<float, 2> GetMousePos() = 0;
    virtual void SetCursorScreenPos(const UI::Vector<float, 2>& pos) = 0;
    
    // --- Search ---
    virtual bool SearchBar(const char* hint, std::string& text) = 0;

    // --- Selectable Combo Box ---
    virtual void SelectableComboBox(const char* label, const std::vector<std::string>& items, int& selectedIdx, UI::Vector<float, 2> size = { 0.0f,0.0f }) = 0;

    // --- Dual List Box ---
    virtual bool DualListBox(const char* label, std::vector<int>& availableItems, std::vector<int>& basketItems, std::function<std::string(int)> getItemNameFn) = 0;

    // --- Hierarchy Tree ---
    virtual bool BeginTreeNode(const char* label, bool isLeaf, bool isSelected) = 0;
    virtual void EndTreeNode() = 0;

    // --- Graphs ---
    virtual void PlotLines(const char* label, const float* values, int count) = 0;
    virtual float GetAvailableWidth() = 0;
    virtual UI::Vector<float, 2> GetRegionAvailable() = 0;
    virtual UI::Vector<float, 2> GetWindowContentMin() = 0;
    virtual UI::Vector<float, 2> GetWindowContentMax() = 0;
    virtual UI::Vector<float, 2> GetCursorScreenPos() = 0;
    virtual UI::Vector<float, 2> GetWindowPos() = 0;
    virtual void Dummy(const UI::Vector<float, 2>& size) = 0;
    virtual void DrawRect(const UI::Vector<float, 2>& p0, const UI::Vector<float, 2>& p1, const UI::Color& color) = 0;
    virtual void DrawRectFilled(const UI::Vector<float, 2>& p0, const UI::Vector<float, 2>& p1, const UI::Color& color) = 0;

    // --- ToopTip ---
    virtual void BeginTooltip() = 0;
    virtual void EndTooltip() = 0;

    // --- Window/Overlay ---
    virtual bool BeginOverlay(const char* name, const UI::Vector<float, 2>& pos, const UI::Vector<float, 2>& size) = 0;
    virtual void EndOverlay() = 0;
    virtual UI::Vector<float, 2> GetMainViewportPos() = 0;
    
    // --- Primitive ---
    virtual void DrawLine(const UI::Vector<float, 2>& p1, const UI::Vector<float, 2>& p2, const UI::Color& color, float thickness = 1.0f) = 0;
    virtual void DrawCircleFilled(const UI::Vector<float, 2>& center, float radius, const UI::Color& color) = 0;
    virtual void DrawTextAt(const UI::Vector<float, 2>& pos, const UI::Color& color, const char* text) = 0;
    virtual UI::Vector<float, 2> CalcTextSize(const char* text) = 0;
    virtual void InvisibleButton(const char* str_id, const UI::Vector<float, 2>& size) = 0;

protected:
    virtual bool InputInternal(const char* label, UI::UI_DataType type, void* pValue) = 0;
    virtual bool DragInternal(const char* label, UI::UI_DataType type, void* pValue, float speed) = 0;
    virtual bool PropertyInternal(const char* label, UI::UI_DataType type, void* pValue, float speed) = 0;
};

