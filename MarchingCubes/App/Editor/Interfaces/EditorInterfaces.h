#pragma once
#include <stdint.h>
#include "Core/Geometry/Mesh.h"

struct IPickable
{
	virtual ~IPickable() = default;
	virtual bool IsPickable() const = 0;
	virtual UINT GetID() const = 0;
	virtual void SetHovered(bool bHovered) = 0;
	virtual bool IsHovered() const = 0;
	virtual void SetSelected(bool bSelected) = 0;
	virtual bool IsSelected() const = 0;
	virtual Mesh* GetMesh() const = 0;
};
