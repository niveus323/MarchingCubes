#include "pch.h"
#include "ImGUIBuilder.h"
#include <imgui_stdlib.h>

bool ImGUIBuilder::BeginPanel(const char* name, bool* pOpen)
{
	return ImGui::Begin(name, pOpen);
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

void ImGUIBuilder::TableNextRow()
{
	ImGui::TableNextRow();
}

void ImGUIBuilder::TableNextColumn()
{
	ImGui::TableNextColumn();
}

bool ImGUIBuilder::BeginCollapsingHeader(const char* label, bool defaultOpen)
{
	ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_AllowItemOverlap;
	if (defaultOpen) flags |= ImGuiTreeNodeFlags_DefaultOpen;

	return ImGui::CollapsingHeader(label, flags);
}

void ImGUIBuilder::Label(const char* text)
{
	ImGui::TextUnformatted(text);
}

bool ImGUIBuilder::Button(const char* label, const UI::Vector2& size)
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

bool ImGUIBuilder::InputInt(const char* label, int* v)
{
	return ImGui::InputInt(label, v);
}

bool ImGUIBuilder::DragFloat(const char* label, float* v, float speed)
{
	return ImGui::DragFloat(label, v, speed);
}

bool ImGUIBuilder::DragFloat3(const char* label, float* v, float speed)
{
	return ImGui::DragFloat3(label, v, speed);
}

bool ImGUIBuilder::EditColor3(const char* label, float* v)
{
	return ImGui::ColorEdit3(label, v);
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

bool ImGUIBuilder::PropertyBool(const char* label, bool* v)
{
	std::string hiddenLabel = DrawPropertyLabel(label);
	return Checkbox(hiddenLabel.c_str(), v);
}

bool ImGUIBuilder::PropertyInt(const char* label, int* v) 
{
	std::string hiddenLabel = DrawPropertyLabel(label);
	return InputInt(hiddenLabel.c_str(), v);
}

bool ImGUIBuilder::PropertyFloat(const char* label, float* v, float speed)
{
	std::string hiddenLabel = DrawPropertyLabel(label);
	return DragFloat(hiddenLabel.c_str(), v, speed);
}

bool ImGUIBuilder::PropertyFloat3(const char* label, float* v, float speed)
{
	std::string hiddenLabel = DrawPropertyLabel(label);
	return DragFloat3(hiddenLabel.c_str(), v, speed);
}

bool ImGUIBuilder::PropertyColor(const char* label, float* v)
{
	std::string hiddenLabel = DrawPropertyLabel(label);
	return EditColor3(hiddenLabel.c_str(), v);
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

bool ImGUIBuilder::IsMouseHoveringRect(const UI::Vector2& pMin, const UI::Vector2& pMax, bool clip)
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

bool ImGUIBuilder::IsKeyPressed_F12()
{
	return ImGui::IsKeyPressed(ImGuiKey_F12);
}

void ImGUIBuilder::SetKeyboardFocus()
{
	ImGui::SetKeyboardFocusHere();
}

bool ImGUIBuilder::SearchBar(const char* hint, std::string& text)
{
	ImGui::SetNextItemWidth(-FLT_MIN);

	// NOTE : 아이콘 추가 시 다음 라인을 주석 해제.
	//ImGui::ImageWithBg();
	//ImGui::SameLine();

	return ImGui::InputTextWithHint("##Search", hint, &text);
}

bool ImGUIBuilder::BeginTreeNode(const char* label, bool isLeaf, bool isSelected)
{
	ImGui::TableNextRow();
	ImGui::TableNextColumn();

	ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanFullWidth;
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

UI::Vector2 ImGUIBuilder::GetCursorScreenPos()
{
	ImVec2 p0 = ImGui::GetCursorScreenPos();
	return UI::Vector2{ p0.x, p0.y };
}

void ImGUIBuilder::Dummy(const UI::Vector2& size)
{
	ImGui::Dummy(ImVec2(size.x, size.y));
}

void ImGUIBuilder::DrawRect(const UI::Vector2& p0, const UI::Vector2& p1, const UI::Color& color)
{
	ImGui::GetWindowDrawList()->AddRect(ImVec2(p0.x, p0.y), ImVec2(p1.x, p1.y), ColorToImU32(color));
}

void ImGUIBuilder::DrawRectFilled(const UI::Vector2& p0, const UI::Vector2& p1, const UI::Color& color)
{
	ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(p0.x, p0.y), ImVec2(p1.x, p1.y), ColorToImU32(color));
}

void ImGUIBuilder::BeginTooltip()
{
	ImGui::BeginTooltip();
}

void ImGUIBuilder::EndTooltip()
{
	ImGui::EndTooltip();
}

bool ImGUIBuilder::BeginOverlay(const char* name, const UI::Vector2& pos, const UI::Vector2& size)
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

UI::Vector2 ImGUIBuilder::GetMainViewportPos()
{
	ImVec2 pos = ImGui::GetMainViewport()->Pos;
	return { pos.x, pos.y };
}

void ImGUIBuilder::DrawLine(const UI::Vector2& p1, const UI::Vector2& p2, const UI::Color& color, float thickness)
{
	ImGui::GetWindowDrawList()->AddLine(
		ImVec2(p1.x, p1.y),
		ImVec2(p2.x, p2.y),
		ColorToImU32(color),
		thickness
	);
}

void ImGUIBuilder::DrawCircleFilled(const UI::Vector2& center, float radius, const UI::Color& color)
{
	ImGui::GetWindowDrawList()->AddCircleFilled(
		ImVec2(center.x, center.y),
		radius,
		ColorToImU32(color)
	);
}

void ImGUIBuilder::DrawTextAt(const UI::Vector2& pos, const UI::Color& color, const char* text)
{
	ImGui::GetWindowDrawList()->AddText(
		ImVec2(pos.x, pos.y),
		ColorToImU32(color),
		text
	);
}

UI::Vector2 ImGUIBuilder::CalcTextSize(const char* text)
{
	ImVec2 size = ImGui::CalcTextSize(text);
	return { size.x, size.y };
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

ImU32 ImGUIBuilder::ColorToImU32(const UI::Color& color)
{
	return IM_COL32(
		(int)(color.r * 255.0f),
		(int)(color.g * 255.0f),
		(int)(color.b * 255.0f),
		(int)(color.a * 255.0f)
	);
}
