#pragma once
#include <stdint.h>

struct IPickable
{
	virtual ~IPickable() = default;
	virtual bool IsPickable() const = 0;
	virtual uint32_t GetID() const = 0;
	virtual void SetHovered(bool bHovered) = 0;
	virtual bool IsHovered() const = 0;

	virtual void RenderForPicking(ID3D12GraphicsCommandList* cmd) const = 0;
};
