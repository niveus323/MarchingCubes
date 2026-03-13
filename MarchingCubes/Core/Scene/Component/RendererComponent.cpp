#include "pch.h"
#include "RendererComponent.h"
#include "Core/Scene/Object/GameObject.h"
#include "Core/Scene/Scene.h"

BEGIN_REFLECTION(RendererComponent, TransformableComponent)
END_REFLECTION()

void RendererComponent::Init()
{
	TransformableComponent::Init();
	if (auto scene = GetOwner()->GetScene())
	{
		scene->RegisterRenderable(this);
	}
}

void RendererComponent::Destroy()
{
	TransformableComponent::Destroy();
	if (auto scene = GetOwner()->GetScene())
	{
		scene->UnregisterRenderable(this);
	}

}

void RendererComponent::AddOverlayPass(const std::string& name, const std::string& psoName, std::vector<ShaderBinding> extraBindings, bool bInitialActive)
{
    for (auto& pass : m_overlayPasses)
    {
        if (pass.name == name)
        {
            pass.psoName = psoName;
            pass.resourceBindings = std::move(extraBindings);
            return;
        }
    }

    // 없으면 새로 생성
    m_overlayPasses.push_back(OverlayPass{
        .name = name,
        .psoName = psoName,
        .bActive = bInitialActive,
        .resourceBindings = std::move(extraBindings)
        });
}

void RendererComponent::SetOverlayPassActive(const std::string& name, bool bActive)
{
    for (auto& pass : m_overlayPasses)
    {
        if (pass.name == name)
        {
            pass.bActive = bActive;
            return;
        }
    }
}

void RendererComponent::RemoveOverlayPass(const std::string& name)
{
    for (auto iter = m_overlayPasses.begin(); iter != m_overlayPasses.end(); ++iter)
    {
        if (iter->name == name)
        {
            m_overlayPasses.erase(iter);
            return;
        }
    }
}