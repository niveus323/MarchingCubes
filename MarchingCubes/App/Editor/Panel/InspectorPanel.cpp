#include "pch.h"
#include "InspectorPanel.h"
#include "Core/Scene/Scene.h"
#include "Core/Scene/Component/Component.h"
#include "Core/UI/Builder/UIBuilder.h"
#include "Core/Assets/ResourceManager.h"
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

            Component* componentToRemove = nullptr;
            for (auto* comp : m_target->GetComponents())
            {
                // Flags 체크 (굳이 노출하지 않아도 될 디버깅 목적 등의 컴포넌트는 배제)
                if (comp->HasAnyFlags(EObjectFlags::Invisible)) continue;

                ui->PushID(comp);
                bool bOpen = ui->CollapsingHeader(comp->GetType()->GetName().c_str());
                if (ui->BeginPopupContextItem())
                {
                    bool bCanDelete = true;
                    std::string dependencyName = "";

                    for (auto* otherComp : m_target->GetComponents())
                    {
                        if (otherComp == comp) continue;

                        auto requiredTypes = otherComp->GetType()->GetRequiredComponents();
                        for (auto* reqType : requiredTypes)
                        {
                            // 다른 컴포넌트가 현재 컴포넌트의 타입을 요구조건으로 가지고 있는지 확인
                            if (comp->GetType() == reqType || comp->GetType()->IsDerivedFrom(reqType->GetName()))
                            {
                                bCanDelete = false;
                                dependencyName = otherComp->GetType()->GetName();
                                break;
                            }
                        }
                        if (!bCanDelete) break;
                    }

                    if (bCanDelete)
                    {
                        if (ui->MenuItem("Remove Component"))
                        {
                            // 즉시 삭제하지 않고 예약
                            componentToRemove = comp;
                        }
                    }
                    else
                    {
                        // 삭제 불가 시 사유를 포함하여 비활성화 텍스트 출력
                        ui->BeginDisabled(true);
                        ui->Text(std::format("Cannot Remove (Required by {})", dependencyName));
                        ui->EndDisabled();
                    }

                    ui->EndPopup();
                }

                if (bOpen)
                {
                    if (ui->BeginTable("ComponentProps", 2))
                    {
                        RenderComponentProperties(ui, reinterpret_cast<void*>(comp), comp->GetType());
                        ui->EndTable();
                    }
                }
                ui->PopID();
            }

            if (componentToRemove)
            {
                m_target->UnregisterComponent(componentToRemove->GetUUID());
            }

            ui->Separator();
            if (ui->Button("Add Component"))
            {
                ui->OpenPopup("AddComponentPopup");
            }

            if (ui->BeginPopup("AddComponentPopup"))
            {
                static std::string searchText = "";
                ui->InputText("Search", searchText);
                ui->Separator();

                std::vector<TypeDescriptor*> componentTypes = ReflectionRegistry::Get().GetTypesDerivedFrom("Component");
                for (TypeDescriptor* typeDesc : componentTypes)
                {
                    std::string typeName = typeDesc->GetName();
                    if (typeName == "Component") continue;

                    if (!searchText.empty() && typeName.find(searchText) == std::string::npos) continue;
                    if (ui->Selectable(typeName.c_str()))
                    {
                        m_target->AddComponent(typeDesc);
                        ui->CloseCurrentPopup();
                    }
                }
                ui->EndPopup();
            }
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
    if (!typeDesc) return;

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
            if (prop.IsIndexedAccessor())
            {
                DrawSingleProperty(ui, componentPtr, prop);
                continue;
            }

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

    if (prop.isArray && prop.IsIndexedAccessor())
    {
        size_t arraySize = prop.getArraySize(instance);

        ui->TableNextRow();
        ui->TableNextColumn();

        bool bOpen = ui->CollapsingHeader(std::format("{} ({} Elements)", name, arraySize).c_str(), true);
        ui->TableNextColumn();

        if (bOpen)
        {
            for (size_t i = 0; i < arraySize; ++i)
            {
                Property elementProp = prop;
                elementProp.name = std::format("[{}]", i); // 라벨 이름: "[0]", "[1]"
                elementProp.isArray = false; // 더 이상 배열이 아님을 명시
                elementProp.getter = [prop, i](void* inst, void* outVal) {
                    prop.indexedGetter(inst, i, outVal);
                };
                elementProp.setter = [prop, i](void* inst, const void* inVal) {
                    prop.indexedSetter(inst, i, inVal);
                };

                DrawSingleProperty(ui, instance, elementProp);
            }
        }
        return;
    }

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
        case EPropertyType::Vector2:
            HandleProperty<UI::Vector<float, 2>>(instance, prop, [&](auto* ptr) {
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
                EnumDescriptor* enumDesc = ReflectionRegistry::Get().GetEnum(prop.metaData);
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
        case EPropertyType::Class:
            HandleProperty<std::string>(instance, prop, [&](std::string* ptr) {
                // 메타데이터(부모 클래스명)를 기반으로 상속된 모든 클래스 조회
                std::vector<TypeDescriptor*> derivedTypes = ReflectionRegistry::Get().GetTypesDerivedFrom(prop.metaData);

                std::vector<std::string> names;
                std::vector<int> values;
                int selectedIndex = 0; // Default: 0번은 None 처리

                names.push_back("None");
                values.push_back(0);

                if (*ptr == "" || *ptr == "None") selectedIndex = 0;

                // 클래스 이름들을 Enum처럼 인덱스로 매핑하여 출력
                for (int i = 0; i < derivedTypes.size(); ++i)
                {
                    std::string className = derivedTypes[i]->GetName();
                    names.push_back(className);
                    values.push_back(i + 1);

                    if (*ptr == className)
                    {
                        selectedIndex = i + 1;
                    }
                }
                bool changed = ui->PropertyEnum(name, &selectedIndex, names, values);
                if (changed && selectedIndex >= 0 && selectedIndex < names.size())
                {
                    if (selectedIndex == 0) *ptr = "";
                    else *ptr = names[selectedIndex];
                }
                return changed;
            });
            break;
        case EPropertyType::Asset:
            HandleProperty<std::string>(instance, prop, [&](std::string* ptr) {
                bool changed = false;
                std::string assetType = prop.metaData; // "Mesh", "Texture" 등
                std::string displayString = ptr->empty() ? "None" : std::filesystem::path(*ptr).filename().string();

                ui->TableNextRow();
                ui->TableNextColumn();
                ui->Text(name);
                ui->TableNextColumn();
                float totalWidth = ui->GetAvailableWidth();
                float clearBtnWidth = 24.0f; // 'X' 버튼용 고정 크기
                float spacing = 4.0f;        // 버튼 사이 여백
                float assetBtnWidth = totalWidth - clearBtnWidth - spacing;
                if (assetBtnWidth < 10.0f) assetBtnWidth = 10.0f; // 최소 너비 방어

                if (ui->Button(std::format("{}##{}", displayString, name).c_str(), { 150.0f, 0.0f }))
                {
                    // TODO: 나중에 에셋 픽커(Asset Picker) 팝업을 띄우는 로직 추가
                }

                if (ui->BeginDragDropTarget())
                {
                    std::string payloadType = "ITEM_" + assetType;
                    Log::Print(ELogVerbosity::Verbose, "InspectorPanel", "payloadType : {}", payloadType);
                    // 허용된 타입의 데이터가 드롭되었는지 확인
                    if (const void* payloadData = ui->AcceptDragDropPayload(payloadType.c_str()))
                    {
                        const char* droppedPath = static_cast<const char*>(payloadData);
                        *ptr = droppedPath;
                        changed = true;
                    }
                    ui->EndDragDropTarget();
                }

                ui->SameLine(0.0f, spacing);

                if (ui->Button(std::format("X##Clear{}", name).c_str()))
                {
                    // 경로 초기화
                    if (!ptr->empty())
                    {
                        *ptr = "";
                        changed = true;
                    }
                }

                return changed;
                });
            break;
    }

}