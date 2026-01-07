#include "pch.h"
#include "InspectorPanel.h"
#include "Core/Scene/Scene.h"
#include "Core/Scene/Component/Component.h"
#include "Core/UI/Builder/UIBuilder.h"
#include <format>

void InspectorPanel::OnRenderUI(IUIBuilder* ui)
{
    if (ui->BeginPanel("Inspector"))
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
            if (ui->BeginCollapsingHeader("Object Settings", true)) // 항상 열어둠
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
                ui->PushID(comp);
                auto* typeDescriptor = comp->GetType();
                if (ui->BeginCollapsingHeader(typeDescriptor->GetName().c_str(), true))
                {
                    if (ui->BeginTable("ComponentProps", 2))
                    {
                        RenderComponentProperties(ui, reinterpret_cast<void*>(comp), typeDescriptor);
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

    if (!typeDesc->GetProperties().empty())
    {
        for (const Property& prop : typeDesc->GetProperties())
        {
            // 가시성 체크, false이면 UI를 그리지 않음
            if (prop.isVisible && !prop.isVisible(componentPtr)) continue;

            bool valueChanged = false;
            //임시 데이터
            bool tempBool = false;
            int tempInt = 0;
            float tempFloat = 0.0f;
            float tempVec3[3] = { 0.f, 0.f, 0.f };
            std::string tempStr = "";
            // UI 함수에 넘길 최종 데이터 포인터
            void* dataPtr = nullptr;

            if (prop.IsAccessor()) //Getter 사용
            {
                switch (prop.type)
                {
                    case EPropertyType::Bool:
                    {
                        prop.getter(componentPtr, &tempBool);
                        dataPtr = &tempBool;
                    }
                    break;
                    case EPropertyType::Int:
                    case EPropertyType::Enum:
                    {
                        prop.getter(componentPtr, &tempInt);
                        dataPtr = &tempInt;
                    }
                    break;
                    case EPropertyType::Float:
                    {
                        prop.getter(componentPtr, &tempFloat);
                        dataPtr = &tempFloat;
                    }
                    break;
                    case EPropertyType::Vector3:
                    {
                        prop.getter(componentPtr, tempVec3);
                        dataPtr = tempVec3;
                    }
                    break;
                    case EPropertyType::String:
                    {
                        prop.getter(componentPtr, &tempStr);
                        dataPtr = &tempStr;
                    }
                    break;
                }
            }
            else // Getter, Setter 없을 경우 직접 포인터 적용
            {
                dataPtr = prop.GetValuePtr(componentPtr);
            }

            // UI 작업
            switch (prop.type)
            {
                case EPropertyType::Bool:
                {
                    if (ui->PropertyBool(prop.name.c_str(), static_cast<bool*>(dataPtr))) valueChanged = true;
                }
                break;
                case EPropertyType::Int:
                {
                    if (ui->PropertyInt(prop.name.c_str(), static_cast<int*>(dataPtr))) valueChanged = true;
                }
                break;
                case EPropertyType::Float:
                {
                    if (ui->PropertyFloat(prop.name.c_str(), static_cast<float*>(dataPtr), 0.1f)) valueChanged = true;
                }
                break;
                case EPropertyType::Vector3:
                {
                    if (ui->PropertyFloat3(prop.name.c_str(), static_cast<float*>(dataPtr), 0.1f)) valueChanged = true;
                }
                break;
                case EPropertyType::String:
                {
                    if (ui->PropertyInputText(prop.name.c_str(), *static_cast<std::string*>(dataPtr))) valueChanged = true;
                }
                break;
                case EPropertyType::Enum:
                {
                    EnumDescriptor* enumDesc = ReflectionRegistry::Get().GetEnum(prop.enumName);
                    if (enumDesc)
                    {
                        std::vector<std::string> names;
                        std::vector<int> values;
                        for (const auto& entry : enumDesc->GetEntries())
                        {
                            names.push_back(entry.name);
                            values.push_back(entry.value);
                        }

                        if (ui->PropertyEnum(prop.name.c_str(), static_cast<int*>(dataPtr), names, values)) valueChanged = true;
                    }
                    else
                    {
                        ui->PropertyText(prop.name.c_str(), "Unknown Enum");
                    }
                }
                break;
            }

            if (prop.IsAccessor() && valueChanged)
            {
                prop.setter(componentPtr, dataPtr);
            }
        }
    }
}
