#include "pch.h"
#include "InspectorPanel.h"
#include "Core/Scene/Scene.h"
#include "Core/Scene/Component/Component.h"
#include "Core/UI/Builder/UIBuilder.h"
#include <format>

namespace
{
    // Property UI 템플릿 함수 (링커 충돌을 방지하기 위해 anonymous namespace로 선언)
    template <typename T, typename DrawFunc>
    void HandleProperty(void* componentPtr, const Property& prop, DrawFunc drawFunc)
    {
        T tempVal = {}; // Accessor용 임시 변수
        T* dataPtr = nullptr;

        if (prop.IsAccessor())
        {
            prop.getter(componentPtr, &tempVal);
            dataPtr = &tempVal;
        }
        else
        {
            dataPtr = static_cast<T*>(prop.GetValuePtr(componentPtr));
        }

        if (drawFunc(dataPtr) && prop.IsAccessor())
        {
            prop.setter(componentPtr, dataPtr);
        }
    }
}

void InspectorPanel::OnRenderUI(IUIBuilder* ui)
{
    if (!m_bShowPanel) return;

    if (ui->BeginPanel("Inspector", &m_bShowPanel))
    {
        if (m_target)
        {
            if (ui->BeginTable("BasicInfo", 2))
            {
                std::string name = m_target->GetName();
                if (ui->PropertyInputText("Name", name))
                {
                    m_target->SetName(name);
                }
                ui->EndTable();
            }
            ui->Separator();

            TypeDescriptor* objectType = m_target->GetType();
            if (ui->CollapsingHeader("Object Settings", true)) // 항상 열어둠
            {
                if (ui->BeginTable("ObjectProps", 2))
                {
                    RenderComponentProperties(ui, m_target, objectType);
                    ui->EndTable();
                }
            }
            ui->Separator();

            for (auto* comp : m_target->GetComponents())
            {
                // Flags 체크 (굳이 노출하지 않아도 될 디버깅 목적 등의 컴포넌트는 배제)
                if (comp->HasAnyFlags(EObjectFlags::Invisible)) continue;

                ui->PushID(comp);
                if (ui->CollapsingHeader(comp->GetName().c_str(), true))
                {
                    if (ui->BeginTable("ComponentProps", 2))
                    {
                        RenderComponentProperties(ui, reinterpret_cast<void*>(comp), comp->GetType());
                        ui->EndTable();
                    }
                }

                ui->PopID();
            }

            //ui->Separator();
            // TODO : 컴포넌트 추가 팝업
            //if (ui->Button("Add Component"))
            //{
            //    ImGui::OpenPopup("AddComponentPopup");
            //}
        }
        else
        {
            ui->Text("No object selected.");
        }
    }
    ui->EndPanel();
}

void InspectorPanel::RenderComponentProperties(IUIBuilder* ui, void* componentPtr, TypeDescriptor* typeDesc)
{
    ui->Text(std::format("Component: {}", typeDesc->GetName()).c_str());
    ui->Separator();

    DrawTypeProperties(ui, componentPtr, typeDesc);
}

void InspectorPanel::DrawTypeProperties(IUIBuilder* ui, void* componentPtr, TypeDescriptor* typeDesc)
{
    if (typeDesc->GetParent())
    {
        DrawTypeProperties(ui, componentPtr, typeDesc->GetParent());
    }

    const auto& properties = typeDesc->GetProperties();
    if (properties.empty()) return;

    for (const Property& prop : typeDesc->GetProperties())
    {
        if (prop.isArray)
        {
            size_t count = prop.getArraySize(componentPtr);
            if (ui->CollapsingHeader(prop.name.c_str()))
            {
                ui->Indent();
                for (size_t i = 0; i < count; ++i)
                {
                    void* elementPtr = prop.getArrayElement(componentPtr, i);
                    // 배열 원소용 임시 Property 생성
                    Property elemProp = prop;
                    elemProp.name = std::format("[{}]", i);
                    elemProp.offset = 0;
                    elemProp.isArray = false;
                    elemProp.getter = nullptr;
                    elemProp.setter = nullptr;
                    DrawSingleProperty(ui, elementPtr, elemProp);
                }
                ui->Unindent();
            }
            continue;
        }

        DrawSingleProperty(ui, componentPtr, prop);  
    }
}

void InspectorPanel::DrawSingleProperty(IUIBuilder* ui, void* instance, const Property& prop)
{
    if (prop.isVisible && !prop.isVisible(instance)) return;

    const char* name = prop.name.c_str();

    switch (prop.type)
    {
        case EPropertyType::Bool:
            HandleProperty<bool>(instance, prop, [&](bool* ptr) {
                return ui->Property(name, ptr);
            });
            break;
        case EPropertyType::Int:
            HandleProperty<int>(instance, prop, [&](int* ptr) {
                return ui->Property(name, ptr);
            });
            break;
        case EPropertyType::Float:
            HandleProperty<float>(instance, prop, [&](float* ptr) {
                return ui->Property(name, ptr);
            });
            break;
        case EPropertyType::Vector3:
            HandleProperty<UI::Vector<float, 3>>(instance, prop, [&](auto* ptr) {
                return ui->Property(name, ptr);
            });
            break;
        case EPropertyType::Color:
            HandleProperty<UI::Color>(instance, prop, [&](auto* ptr) {
                return ui->Property(name, ptr);
            });
            break;
        case EPropertyType::String:
            HandleProperty<std::string>(instance, prop, [&](std::string* ptr) {
                return ui->PropertyInputText(name, *ptr);
            });
            break;
        case EPropertyType::Enum:
            // Enum은 HandleProperty를 활용하되 내부에서 메타데이터 검색
            HandleProperty<int>(instance, prop, [&](int* ptr) {
                EnumDescriptor* enumDesc = ReflectionRegistry::Get().GetEnum(prop.enumName);
                if (!enumDesc) return false;
                // NOTE : 최적화 필요 시 캐싱 혹은 static 변수로 선언할것.
                std::vector<std::string> names;
                std::vector<int> values;
                for (const auto& entry : enumDesc->GetEntries())
                {
                    names.push_back(entry.name);
                    values.push_back(entry.value);
                }
                return ui->PropertyEnum(name, ptr, names, values);
            });
            break;
    }

}