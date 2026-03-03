#include "pch.h"
#include "ImGUIBuilder.h"
#include <imgui.h>
#include <imgui_stdlib.h>
#include <cstdarg>
#include <unordered_map>
#include <algorithm>
#include "ThirdParty/ImGuizmo/ImGuizmo.h"

namespace UI
{
	ImGuiDataType GetImGuiDataType(UI::UI_DataType type)
	{
		switch (type) {
			case UI_DataType::Int:  
			case UI_DataType::Int2:  
			case UI_DataType::Int3:  
				return ImGuiDataType_S32;

			case UI_DataType::UInt:   
			case UI_DataType::UInt2:   
			case UI_DataType::UInt3:
				return ImGuiDataType_U32;

			case UI_DataType::Float: 
			case UI_DataType::Float2: 
			case UI_DataType::Float3: 
			case UI_DataType::Float4:
				return ImGuiDataType_Float;	
		}
		return ImGuiDataType_Float;
	}

	struct DualListBoxState
	{
		ImGuiSelectionBasicStorage selections[2];
		bool optKeepSorted = true;
	};

}

bool ImGUIBuilder::BeginPanel(const char* name, bool* pOpen, UI::UI_PanelOption flags)
{
	ImGuiWindowFlags windowFlag = ImGuiWindowFlags_None;
	if (HasFlag(flags, UI::UI_PanelOption::MenuBar)) windowFlag |= ImGuiWindowFlags_MenuBar;
	if (HasFlag(flags, UI::UI_PanelOption::NoDocking)) windowFlag |= ImGuiWindowFlags_NoDocking;
	if (HasFlag(flags, UI::UI_PanelOption::NoInput)) windowFlag |= ImGuiWindowFlags_NoInputs;
	if (HasFlag(flags, UI::UI_PanelOption::NoMove)) windowFlag |= ImGuiWindowFlags_NoMove;
	if (HasFlag(flags, UI::UI_PanelOption::NoScrollBar)) windowFlag |= ImGuiWindowFlags_NoScrollbar;
	if (HasFlag(flags, UI::UI_PanelOption::NoTitleBar)) windowFlag |= ImGuiWindowFlags_NoTitleBar;
	if (HasFlag(flags, UI::UI_PanelOption::NoCollapse)) windowFlag |= ImGuiWindowFlags_NoCollapse;
	return ImGui::Begin(name, pOpen, windowFlag);
}

void ImGUIBuilder::EndPanel()
{
	ImGui::End();
}

bool ImGUIBuilder::BeginTable(const char* id, int columns)
{
	return ImGui::BeginTable(id, columns, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV);
}

void ImGUIBuilder::EndTable()
{
	ImGui::EndTable();
}

bool ImGUIBuilder::CollapsingHeader(const char* label, bool defaultOpen)
{
	ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_AllowItemOverlap;
	if (defaultOpen) flags |= ImGuiTreeNodeFlags_DefaultOpen;

	return ImGui::CollapsingHeader(label, flags);
}

bool ImGUIBuilder::BeginTabBar(const char* id)
{
	ImGuiTabBarFlags flags = ImGuiTabBarFlags_FittingPolicyScroll	// 탭 크기 유지 + 좌우 스크롤
		| ImGuiTabBarFlags_NoCloseWithMiddleMouseButton;			// 마우스 중앙 버튼 클릭으로 탭 닫기X
	return ImGui::BeginTabBar(id, flags);
}

void ImGUIBuilder::EndTabBar()
{
	ImGui::EndTabBar();
}

bool ImGUIBuilder::BeginTabItem(const char* id, bool* pOpen)
{
	ImGuiTabItemFlags flags = ImGuiTabItemFlags_NoCloseWithMiddleMouseButton;
	return ImGui::BeginTabItem(id, pOpen, flags);
}

void ImGUIBuilder::EndTabItem()
{
	ImGui::EndTabItem();
}

void ImGUIBuilder::BeginMainMenuBar()
{
	ImGui::BeginMainMenuBar();
}

void ImGUIBuilder::EndMainMenuBar()
{
	ImGui::EndMainMenuBar();
}

bool ImGUIBuilder::BeginMenuBar()
{
	return ImGui::BeginMenuBar();
}

void ImGUIBuilder::EndMenuBar()
{
	ImGui::EndMenuBar();
}

bool ImGUIBuilder::BeginMenu(const char* id)
{
	return ImGui::BeginMenu(id);
}

void ImGUIBuilder::EndMenu()
{
	ImGui::EndMenu();
}

bool ImGUIBuilder::MenuItem(const char* id, const char* shortcutKey, bool bSelected)
{
	return ImGui::MenuItem(id, shortcutKey, bSelected);
}

void ImGUIBuilder::BeginDisabled(bool disabled)
{
	ImGui::BeginDisabled(disabled);
}

void ImGUIBuilder::EndDisabled()
{
	ImGui::EndDisabled();
}

void ImGUIBuilder::Label(const char* text)
{
	ImGui::TextUnformatted(text);
}

bool ImGUIBuilder::Button(const char* label, const UI::Vector<float, 2>& size)
{
	return ImGui::Button(label, ImVec2(size.x, size.y));
}

bool ImGUIBuilder::Checkbox(const char* label, bool* v)
{
	return ImGui::Checkbox(label, v);
}

void ImGUIBuilder::Text(const char* text)
{
	ImGui::TextUnformatted(text);
}

void ImGUIBuilder::Text(const std::string& text)
{
	ImGui::TextUnformatted(text.c_str());
}

void ImGUIBuilder::TextFormatted(const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt); // fmt 이후의 인자들을 args로 수집
	ImGui::TextV(fmt, args); // ImGui의 va_list 지원 함수 호출
	va_end(args); // 리소스 정리
}

void ImGUIBuilder::TextColored(const UI::Color& color, const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	ImGui::TextColoredV(ImVec4(color.r, color.g, color.b, color.a), fmt, args);
	va_end(args);
}

// NOTE : SRV 힙 내의 위치를 기반으로 핸들 생성 필요
void ImGUIBuilder::Image(void* textureHandle, const UI::Vector<float, 2>& size)
{
	ImGui::Image((ImTextureID)textureHandle, ImVec2(size.x, size.y));
}

bool ImGUIBuilder::InputText(const char* label, std::string& text)
{
	return ImGui::InputText(label, &text, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
}

bool ImGUIBuilder::InputEnum(const char* label, int* currentValue, const std::vector<std::string>& names, const std::vector<int>& values)
{
	// 현재 값이 리스트 중 몇 번째인지 찾기 (Index)
	int currentItemIndex = 0;
	for (int i = 0; i < values.size(); ++i)
	{
		if (values[i] == *currentValue)
		{
			currentItemIndex = i;
			break;
		}
	}

	// ImGui 콤보 박스 그리기
	bool changed = false;
	const char* previewValue = names[currentItemIndex].c_str();

	if (ImGui::BeginCombo(label, previewValue))
	{
		for (int i = 0; i < names.size(); ++i)
		{
			bool isSelected = (currentItemIndex == i);
			if (ImGui::Selectable(names[i].c_str(), isSelected))
			{
				*currentValue = values[i]; // 선택된 Enum 값 적용
				changed = true;
			}
			if (isSelected) ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}

	return changed;
}

void ImGUIBuilder::TableHeadersRow()
{
	ImGui::TableHeadersRow();
}

void ImGUIBuilder::TableNextRow()
{
	ImGui::TableNextRow();
}

void ImGUIBuilder::TableNextColumn()
{
	ImGui::TableNextColumn();
}

void ImGUIBuilder::TableSetColumnIndex(int index)
{
	ImGui::TableSetColumnIndex(index);
}

void ImGUIBuilder::TableSetupColumn(const char* id)
{
	ImGui::TableSetupColumn(id);
}

void ImGUIBuilder::PropertyText(const char* label, const char* value)
{
	ImGui::TableNextRow();
	ImGui::TableNextColumn();
	ImGui::AlignTextToFramePadding();
	ImGui::TextUnformatted(label);
	ImGui::TableNextColumn();
	ImGui::SetNextItemWidth(-FLT_MIN);
	Text(value);
}

bool ImGUIBuilder::PropertyInputText(const char* label, std::string& text)
{
	ImGui::TableNextRow();
	ImGui::TableNextColumn();
	ImGui::SetNextItemWidth(-FLT_MIN);
	return InputText(label, text);
}

bool ImGUIBuilder::PropertyEnum(const char* label, int* currentValue, const std::vector<std::string>& names, const std::vector<int>& values)
{
	std::string hiddenLabel = DrawPropertyLabel(label);
	return InputEnum(hiddenLabel.c_str(), currentValue, names, values);
}

void ImGUIBuilder::Separator()
{
	ImGui::Separator();
}

void ImGUIBuilder::SameLine(float offset, float spacing)
{
	ImGui::SameLine(offset, spacing);
}

void ImGUIBuilder::Indent(float width)
{
	ImGui::Indent(width);
}

void ImGUIBuilder::Unindent(float width)
{
	ImGui::Unindent(width);
}

void ImGUIBuilder::AlignNextItem(UI::UI_Alignment align, float itemWidth)
{
	float nextItemPos = ImGui::GetCursorPosX();
	switch (align)
	{
		case UI::UI_Alignment::AlignCenter:
			nextItemPos += (ImGui::GetContentRegionAvail().x - itemWidth) * 0.5f;
			break;
		case UI::UI_Alignment::AlignRight:
			nextItemPos += (ImGui::GetContentRegionAvail().x - itemWidth);
			break;
		case UI::UI_Alignment::AlignLeft:
		default:
			return;
	}
	ImGui::SetCursorPosX(nextItemPos);
}

void ImGUIBuilder::PushStyle_Padding(const UI::Vector<float, 2>& padding)
{
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(padding.x, padding.y));
}

void ImGUIBuilder::PopStyle(int count)
{
	ImGui::PopStyleVar(count);
}

void ImGUIBuilder::SetNextItemWidth(float width)
{
	ImGui::SetNextItemWidth(width);
}

void ImGUIBuilder::PushID(const char* str_id)
{
	ImGui::PushID(str_id);
}

void ImGUIBuilder::PushID(const void* ptr_id)
{
	ImGui::PushID(ptr_id);
}

void ImGUIBuilder::PopID()
{
	ImGui::PopID();
}

bool ImGUIBuilder::IsItemClicked()
{
	return ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen();
}

bool ImGUIBuilder::IsItemHovered()
{
	return ImGui::IsItemHovered();
}

bool ImGUIBuilder::IsItemActive()
{
	return ImGui::IsItemActive();
}

bool ImGUIBuilder::IsMouseHoveringRect(const UI::Vector<float, 2>& pMin, const UI::Vector<float, 2>& pMax, bool clip)
{
	return ImGui::IsMouseHoveringRect(ImVec2(pMin.x, pMin.y), ImVec2(pMax.x, pMax.y), clip);
}

bool ImGUIBuilder::IsAnyItemHovered()
{
	return ImGui::IsAnyItemHovered();
}

bool ImGUIBuilder::IsItemDeactivated()
{
	return ImGui::IsItemDeactivated();
}

bool ImGUIBuilder::IsWindowFocused()
{
	return ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
}

bool ImGUIBuilder::IsWindowHovered()
{
	return ImGui::IsWindowHovered();
}

bool ImGUIBuilder::IsKeyPressed_F12()
{
	return ImGui::IsKeyPressed(ImGuiKey_F12);
}

void ImGUIBuilder::SetKeyboardFocus()
{
	ImGui::SetKeyboardFocusHere();
}

bool ImGUIBuilder::IsMouseClicked(int button)
{
	return ImGui::IsMouseClicked(button);
}

bool ImGUIBuilder::IsMouseReleased(int button)
{
	return ImGui::IsMouseReleased(button);
}

bool ImGUIBuilder::IsMouseDragging(int button)
{
	return ImGui::IsMouseDragging(button);
}

UI::Vector<float, 2> ImGUIBuilder::GetMousePos()
{
	auto pos = ImGui::GetMousePos();
	return UI::Vector<float, 2>(pos.x, pos.y);
}

void ImGUIBuilder::SetCursorScreenPos(const UI::Vector<float, 2>& pos)
{
	ImGui::SetCursorScreenPos(ImVec2(pos.x, pos.y));
}

bool ImGUIBuilder::SearchBar(const char* hint, std::string& text)
{
	ImGui::SetNextItemWidth(-FLT_MIN);

	// NOTE : 아이콘 추가 시 다음 라인을 주석 해제.
	//ImGui::ImageWithBg();
	//ImGui::SameLine();

	return ImGui::InputTextWithHint("##Search", hint, &text);
}

void ImGUIBuilder::SelectableComboBox(const char* label, const std::vector<std::string>& items, int& selectedIdx, UI::Vector<float, 2> size)
{
	std::string hiddenLabel = "##";
	hiddenLabel += label;

	if (ImGui::BeginListBox(hiddenLabel.c_str(), ImVec2(size.x, size.y)))
	{
		for (int n = 0; n < items.size(); n++)
		{
			const bool is_selected = (selectedIdx == n);
			if (ImGui::Selectable(items[n].c_str(), is_selected))
				selectedIdx = n;

			// Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
			if (is_selected)
				ImGui::SetItemDefaultFocus();
		}
		ImGui::EndListBox();
	}
}

bool ImGUIBuilder::DualListBox(const char* label, std::vector<int>& availableItems, std::vector<int>& basketItems, std::function<std::string(int)> getItemNameFn)
{
	bool dataChanged = false;

	ImGuiID id = ImGui::GetID(label);

	// ID마다 고유한 상태 관리를 위해 정적 변수로 정의 및 관리
	static std::unordered_map<ImGuiID, UI::DualListBoxState> s_states;
	UI::DualListBoxState& state = s_states[id];

	ImGui::PushID(label);

	std::vector<int>* items[2] = { &availableItems, &basketItems };

	// 이동 함수
	auto MoveAll = [&](int src, int dst) {
		for (int item_id : *items[src]) items[dst]->push_back(item_id);
		items[src]->clear();
		if (state.optKeepSorted) std::sort(items[dst]->begin(), items[dst]->end());

		state.selections[src].Swap(state.selections[dst]);
		state.selections[src].Clear();
		dataChanged = true;
	};

	auto MoveSelected = [&](int src, int dst) {
		for (auto it = items[src]->begin(); it != items[src]->end(); )
		{
			if (state.selections[src].Contains((ImGuiID)*it))
			{
				items[dst]->push_back(*it);
				it = items[src]->erase(it);
				dataChanged = true;
			}
			else { ++it; }
		}
		if (state.optKeepSorted) std::sort(items[dst]->begin(), items[dst]->end());

		state.selections[src].Swap(state.selections[dst]);
		state.selections[src].Clear();
	};

	auto ApplySelectionRequests = [&](ImGuiMultiSelectIO* ms_io, int side) {
		state.selections[side].UserData = items[side]->data();
		state.selections[side].AdapterIndexToStorageId = [](ImGuiSelectionBasicStorage* self, int idx) {
			return (ImGuiID)((int*)self->UserData)[idx];
		};
		state.selections[side].ApplyRequests(ms_io);
	};

	if (ImGui::BeginTable("DualListBoxTable", 3))
	{
		ImGui::TableSetupColumn("Available", 0);
		ImGui::TableSetupColumn("Controls", ImGuiTableColumnFlags_WidthFixed, 40.0f);
		ImGui::TableSetupColumn("Basket", 0);
		ImGui::TableNextRow();

		int request_move_selected = -1;
		int request_move_all = -1;
		float child_height_0 = 0.0f;

		for (int side = 0; side < 2; side++)
		{
			ImGui::TableSetColumnIndex((side == 0) ? 0 : 2);
			ImGui::Text("%s (%zu)", (side == 0) ? "Available" : "Basket", items[side]->size());

			const float items_height = ImGui::GetTextLineHeightWithSpacing();
			ImGui::SetNextWindowContentSize(ImVec2(0.0f, items[side]->size() * items_height));

			bool child_visible;
			if (side == 0)
			{
				ImGui::SetNextWindowSizeConstraints(ImVec2(0.0f, ImGui::GetFrameHeightWithSpacing() * 4), ImVec2(FLT_MAX, FLT_MAX));
				child_visible = ImGui::BeginChild("0", ImVec2(-FLT_MIN, ImGui::GetFontSize() * 15), ImGuiChildFlags_FrameStyle | ImGuiChildFlags_ResizeY);
				child_height_0 = ImGui::GetWindowSize().y;
			}
			else
			{
				child_visible = ImGui::BeginChild("1", ImVec2(-FLT_MIN, child_height_0), ImGuiChildFlags_FrameStyle);
			}

			if (child_visible)
			{
				ImGuiMultiSelectIO* ms_io = ImGui::BeginMultiSelect(ImGuiMultiSelectFlags_None, state.selections[side].Size, (int)items[side]->size());
				ApplySelectionRequests(ms_io, side);

				for (size_t item_n = 0; item_n < items[side]->size(); item_n++)
				{
					int item_id = (*items[side])[item_n];
					bool item_is_selected = state.selections[side].Contains((ImGuiID)item_id);

					ImGui::SetNextItemSelectionUserData(item_n);
					std::string itemName = getItemNameFn(item_id);

					ImGui::Selectable(itemName.c_str(), item_is_selected, ImGuiSelectableFlags_AllowDoubleClick);

					if (ImGui::IsItemFocused())
					{
						if (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter) || ImGui::IsMouseDoubleClicked(0))
							request_move_selected = side;
					}
				}

				ms_io = ImGui::EndMultiSelect();
				ApplySelectionRequests(ms_io, side);
			}
			ImGui::EndChild();
		}

		ImGui::TableSetColumnIndex(1);
		ImGui::Dummy({ 0, ImGui::GetTextLineHeight() });

		ImVec2 button_sz = { ImGui::GetFrameHeight(), ImGui::GetFrameHeight() };

		if (ImGui::Button(">>", button_sz)) request_move_all = 0;
		if (ImGui::Button(">", button_sz))  request_move_selected = 0;
		if (ImGui::Button("<", button_sz))  request_move_selected = 1;
		if (ImGui::Button("<<", button_sz)) request_move_all = 1;

		if (request_move_all != -1) MoveAll(request_move_all, request_move_all ^ 1);
		if (request_move_selected != -1) MoveSelected(request_move_selected, request_move_selected ^ 1);

		ImGui::EndTable();
	}

	ImGui::PopID();
	return dataChanged;
}

bool ImGUIBuilder::BeginTreeNode(const char* label, bool isLeaf, bool isSelected)
{
	ImGui::TableNextRow();
	ImGui::TableNextColumn();

	ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth;
	if (isSelected) flags |= ImGuiTreeNodeFlags_Selected;
	if (isLeaf)     flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_Bullet;

	return ImGui::TreeNodeEx(label, flags);
}

void ImGUIBuilder::EndTreeNode()
{
	ImGui::TreePop();
}

void ImGUIBuilder::PlotLines(const char* label, const float* values, int count)
{
	ImGui::PlotLines(label, values, count);
}

float ImGUIBuilder::GetAvailableWidth()
{
	return ImGui::GetContentRegionAvail().x;
}

UI::Vector<float, 2> ImGUIBuilder::GetRegionAvailable()
{
	ImVec2 size = ImGui::GetContentRegionAvail();
	return UI::Vector<float, 2>(size.x, size.y);
}

UI::Vector<float, 2> ImGUIBuilder::GetWindowContentMin()
{
	ImVec2 min = ImGui::GetWindowContentRegionMin();
	return UI::Vector<float, 2>(min.x, min.y);
}

UI::Vector<float, 2> ImGUIBuilder::GetWindowContentMax()
{
	ImVec2 max = ImGui::GetWindowContentRegionMax();
	return UI::Vector<float, 2>(max.x, max.y);
}

UI::Vector<float, 2> ImGUIBuilder::GetCursorScreenPos()
{
	ImVec2 p0 = ImGui::GetCursorScreenPos();
	return UI::Vector<float, 2>(p0.x, p0.y);
}

UI::Vector<float, 2> ImGUIBuilder::GetWindowPos()
{
	ImVec2 p0 = ImGui::GetWindowPos();
	return UI::Vector<float, 2>(p0.x, p0.y);
}

void ImGUIBuilder::Dummy(const UI::Vector<float, 2>& size)
{
	ImGui::Dummy(ImVec2(size.x, size.y));
}

void ImGUIBuilder::DrawRect(const UI::Vector<float, 2>& p0, const UI::Vector<float, 2>& p1, const UI::Color& color)
{
	ImGui::GetWindowDrawList()->AddRect(ImVec2(p0.x, p0.y), ImVec2(p1.x, p1.y), color.ToUInt());
}

void ImGUIBuilder::DrawRectFilled(const UI::Vector<float, 2>& p0, const UI::Vector<float, 2>& p1, const UI::Color& color)
{
	ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(p0.x, p0.y), ImVec2(p1.x, p1.y), color.ToUInt());
}

void ImGUIBuilder::BeginTooltip()
{
	ImGui::BeginTooltip();
}

void ImGUIBuilder::EndTooltip()
{
	ImGui::EndTooltip();
}

bool ImGUIBuilder::BeginOverlay(const char* name, const UI::Vector<float, 2>& pos, const UI::Vector<float, 2>& size)
{
	ImGui::SetNextWindowPos(ImVec2(pos.x, pos.y), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(size.x, size.y));
	ImGui::SetNextWindowBgAlpha(0.0f);

	// Padding 제거 스타일 적용
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

	return ImGui::Begin(name, nullptr,
		ImGuiWindowFlags_NoDecoration |
		ImGuiWindowFlags_NoInputs |
		ImGuiWindowFlags_NoBackground |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoFocusOnAppearing |
		ImGuiWindowFlags_NoNav);
}

void ImGUIBuilder::EndOverlay()
{
	ImGui::End();
	ImGui::PopStyleVar(1); // WindowPadding 복구
}

UI::Vector<float, 2> ImGUIBuilder::GetMainViewportPos()
{
	ImVec2 pos = ImGui::GetMainViewport()->Pos;
	return { pos.x, pos.y };
}

void ImGUIBuilder::DockSpaceOverViewport(const char* dockSpaceId, const void* viewport)
{
	const ImGuiViewport* targetViewport = viewport ? static_cast<const ImGuiViewport*>(viewport) : ImGui::GetMainViewport();

	ImGuiID id = (dockSpaceId)? ImGui::GetID(dockSpaceId) : 0;
	ImGui::DockSpaceOverViewport(id, ImGui::GetMainViewport(), ImGuiDockNodeFlags_None);
}

void ImGUIBuilder::SetNextWindowDocking(const char* dockspaceId, UI::UI_Condition cond, UI::UI_DockingOption option)
{
	ImGuiCond imguiCond = ImGuiCond_None;
	switch (cond)
	{
		case UI::UI_Condition::FirstUseEver: 
			imguiCond = ImGuiCond_FirstUseEver; break;
		case UI::UI_Condition::Always:       
			imguiCond = ImGuiCond_Always; break;
		case UI::UI_Condition::Once:         
			imguiCond = ImGuiCond_Once; break;
		case UI::UI_Condition::Appearing:    
		default:
			imguiCond = ImGuiCond_Appearing; break;
	}
	ImGui::SetNextWindowDockID(ImGui::GetID(dockspaceId), imguiCond);

	if (option != UI::UI_DockingOption::None)
	{
		static std::unordered_map<uint32_t, ImGuiWindowClass> s_windowClasses;
		uint32_t optionKey = static_cast<uint32_t>(option);
		if (s_windowClasses.find(optionKey) == s_windowClasses.end())
		{
			ImGuiWindowClass& custom_class = s_windowClasses[optionKey];
			custom_class.ClassId = ImGui::GetID("CustomDockClass");
			custom_class.DockNodeFlagsOverrideSet = 0;
			if (HasFlag(option, UI::UI_DockingOption::NoTabBar))	custom_class.DockNodeFlagsOverrideSet |= ImGuiDockNodeFlags_AutoHideTabBar;
			if (HasFlag(option, UI::UI_DockingOption::NoUndocking)) custom_class.DockNodeFlagsOverrideSet |= ImGuiDockNodeFlags_NoUndocking;
			if (HasFlag(option, UI::UI_DockingOption::NoSplit))		custom_class.DockNodeFlagsOverrideSet |= ImGuiDockNodeFlags_NoSplit;
			if (HasFlag(option, UI::UI_DockingOption::NoResize))	custom_class.DockNodeFlagsOverrideSet |= ImGuiDockNodeFlags_NoResize;

		}		
		ImGui::SetNextWindowClass(&s_windowClasses[optionKey]);
	}
}

void ImGUIBuilder::DrawLine(const UI::Vector<float, 2>& p1, const UI::Vector<float, 2>& p2, const UI::Color& color, float thickness)
{
	ImGui::GetWindowDrawList()->AddLine(
		ImVec2(p1.x, p1.y),
		ImVec2(p2.x, p2.y),
		color.ToUInt(),
		thickness
	);
}

void ImGUIBuilder::DrawCircleFilled(const UI::Vector<float, 2>& center, float radius, const UI::Color& color)
{
	ImGui::GetWindowDrawList()->AddCircleFilled(
		ImVec2(center.x, center.y),
		radius,
		color.ToUInt()
	);
}

void ImGUIBuilder::DrawTextAt(const UI::Vector<float, 2>& pos, const UI::Color& color, const char* text)
{
	ImGui::GetWindowDrawList()->AddText(
		ImVec2(pos.x, pos.y),
		color.ToUInt(),
		text
	);
}

UI::Vector<float, 2> ImGUIBuilder::CalcTextSize(const char* text)
{
	ImVec2 size = ImGui::CalcTextSize(text);
	return { size.x, size.y };
}

void ImGUIBuilder::InvisibleButton(const char* str_id, const UI::Vector<float, 2>& size)
{
	ImGui::InvisibleButton(str_id, ImVec2(size.x, size.y));
}

bool ImGUIBuilder::IsGizmoHovered()
{
	return ImGuizmo::IsOver();
}
_Success_(return)
bool ImGUIBuilder::DrawTransformGizmo(const DirectX::XMFLOAT4X4& view, const DirectX::XMFLOAT4X4& proj, UI::EGizmoOperation op, UI::EGizmoMode mode, _Inout_ DirectX::XMFLOAT4X4& world, _Out_ DirectX::XMFLOAT3& translation, _Out_ DirectX::XMFLOAT3& rotation, _Out_ DirectX::XMFLOAT3& scale, float gizmoSize)
{
	ImGuizmo::SetOrthographic(false);
	ImGuizmo::SetDrawlist();
	ImGuizmo::AllowAxisFlip(false);
	ImGuizmo::SetGizmoSizeClipSpace(gizmoSize);

	// Viewport 영역 확인
	ImVec2 vPos = ImGui::GetWindowPos();
	ImVec2 vMin = ImGui::GetWindowContentRegionMin();
	ImVec2 vMax = ImGui::GetWindowContentRegionMax();
	ImVec2 contentStartPos = ImVec2(vPos.x + vMin.x, vPos.y + vMin.y);
	ImVec2 contentSize = ImVec2(vMax.x - vMin.x, vMax.y - vMin.y);
	ImGuizmo::SetRect(contentStartPos.x, contentStartPos.y, contentSize.x, contentSize.y);

	ImGuizmo::OPERATION gizmoOp = 
		(op == UI::EGizmoOperation::Translate) ? ImGuizmo::TRANSLATE :
		(op == UI::EGizmoOperation::Rotate) ? ImGuizmo::ROTATE : 
		ImGuizmo::SCALE;
	ImGuizmo::MODE gizmoMode = (mode == UI::EGizmoMode::Local) ? ImGuizmo::LOCAL : ImGuizmo::WORLD;

	float textScaleMulti = 1.25f;
	ImGui::SetWindowFontScale(textScaleMulti);

	bool bManipulated = ImGuizmo::Manipulate(*view.m, *proj.m, gizmoOp, gizmoMode, *world.m) && ImGuizmo::IsUsing();
	if (bManipulated)
	{
		float t[3], r[3], s[3];
		ImGuizmo::DecomposeMatrixToComponents(*world.m, t, r, s);

		translation = XMFLOAT3( t[0], t[1], t[2] );
		rotation = XMFLOAT3( r[0], r[1], r[2] );
		scale = XMFLOAT3( s[0], s[1], s[2] );
	}

	ImGui::SetWindowFontScale(1.0f);
	return bManipulated;
}

bool ImGUIBuilder::InputInternal(const char* label, UI::UI_DataType type, void* pValue)
{
	if (type == UI::UI_DataType::Unknown) return false;
	if (type == UI::UI_DataType::Bool) return ImGui::Checkbox(label, static_cast<bool*>(pValue));
	if (type == UI::UI_DataType::Color) return ImGui::ColorEdit4(label, static_cast<float*>(pValue), ImGuiColorEditFlags_NoDragDrop);

	ImGuiDataType imguiType = GetImGuiDataType(type);
	int components = GetComponentCount(type);
	return ImGui::InputScalarN(label, imguiType, pValue, components);
}

bool ImGUIBuilder::DragInternal(const char* label, UI::UI_DataType type, void* pValue, float speed)
{
	if (type == UI::UI_DataType::Unknown) return false;
	if (type == UI::UI_DataType::Bool) return ImGui::Checkbox(label, static_cast<bool*>(pValue));
	if (type == UI::UI_DataType::Color) return ImGui::ColorEdit4(label, static_cast<float*>(pValue));

	ImGuiDataType imguiType = GetImGuiDataType(type);
	int components = GetComponentCount(type);
	return ImGui::DragScalarN(label, imguiType, pValue, components, speed);
}

bool ImGUIBuilder::PropertyInternal(const char* label, UI::UI_DataType type, void* pValue, float speed)
{
	std::string hiddenLabel = DrawPropertyLabel(label);
	return DragInternal(hiddenLabel.c_str(), type, pValue, speed);
}

std::string ImGUIBuilder::DrawPropertyLabel(const char* label)
{
	ImGui::TableNextRow();
	ImGui::TableNextColumn();
	ImGui::AlignTextToFramePadding();
	ImGui::TextUnformatted(label);
	ImGui::TableNextColumn();

	ImGui::SetNextItemWidth(-FLT_MIN);
	std::string hiddenLabel = "##";
	hiddenLabel += label;
	return hiddenLabel;
}

