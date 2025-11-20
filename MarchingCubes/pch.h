#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif // !WIN32_LEAN_AND_MEAN

#include <windows.h>

#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>

#include <string>
#include <wrl.h>

#include <shellapi.h>

//DirectX Helper https://github.com/microsoft/DirectX-Headers
#include "DirectX-Headers/include/directx/d3dx12.h"
#include "Core/Utils/DXHelper.h"
#include "Core/Trace/Log.h"

#define PIX_DEBUGMODE 0

#if PIX_DEBUGMODE
#include <pix3.h>

static HMODULE sPixGPU = PIXLoadLatestWinPixGpuCapturerLibrary();
//static HMODULE sPixTiming = PIXLoadLatestWinPixTimingCapturerLibrary();
#endif
