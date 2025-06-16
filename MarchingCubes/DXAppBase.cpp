#include "pch.h"
#include "DXAppBase.h"
using namespace Microsoft::WRL;

DXAppBase::DXAppBase(UINT width, UINT height, std::wstring name) : 
	m_width(width), 
	m_height(height), 
	m_title(name), 
	m_userWarpDevice(false)
{

	m_aspectRatio = static_cast<float>(width) / static_cast<float>(height);

}

DXAppBase::~DXAppBase()
{
}

_Use_decl_annotations_
void DXAppBase::ParseCommandLineArgs(WCHAR* argv[], int argc)
{
	for (int i = 1; i < argc; ++i)
	{
		if (_wcsnicmp(argv[i], L"-warp", wcslen(argv[i])) == 0 ||
			_wcsnicmp(argv[i], L"/warp", wcslen(argv[i])) == 0)
		{
			m_userWarpDevice = true;
			m_title = m_title + L" (WARP)";
		}
	}
}

void DXAppBase::StartTimer()
{
	m_timer.Start();
}

void DXAppBase::TickAndUpdate()
{
	float deltaTime = m_timer.Tick();
	OnUpdate(deltaTime);
}

std::wstring DXAppBase::GetAssetFullPath(LPCWSTR assetName)
{
	return GetFullPath(AssetType::Default, assetName);
}

std::wstring DXAppBase::GetShaderFullPath(LPCWSTR shaderName)
{
	return GetFullPath(AssetType::Shader, shaderName);
}

_Use_decl_annotations_
void DXAppBase::GetHawrdwardAdapter(IDXGIFactory1* pFactory, IDXGIAdapter1** ppAdapter, bool requestHightPerformanceAdapter)
{
	*ppAdapter = nullptr;

	ComPtr<IDXGIAdapter1> adapter;

	ComPtr<IDXGIFactory6> factory6;
	
	if (SUCCEEDED(pFactory->QueryInterface(IID_PPV_ARGS(&factory6))))
	{
		for (UINT adapterIndex = 0; SUCCEEDED(factory6->EnumAdapterByGpuPreference(adapterIndex, requestHightPerformanceAdapter == true ? DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE : DXGI_GPU_PREFERENCE_UNSPECIFIED, IID_PPV_ARGS(&adapter))); ++adapterIndex)
		{
			DXGI_ADAPTER_DESC1 desc;
			adapter->GetDesc1(&desc);

			if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
			{
				continue;
			}

			if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), nullptr)))
			{
				break;
			}
		}
	}

	if (adapter.Get() == nullptr)
	{
		for (UINT adapterIndex = 0; SUCCEEDED(pFactory->EnumAdapters1(adapterIndex, &adapter)); ++adapterIndex)
		{
			DXGI_ADAPTER_DESC1 desc;
			adapter->GetDesc1(&desc);

			if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
			{
				continue;
			}

			if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), nullptr)))
			{
				break;
			}
		}
	}

	*ppAdapter = adapter.Detach();
}

void DXAppBase::SetCustomWindowText(LPCWSTR text)
{
	std::wstring windowText = m_title + L": " + text;
	SetWindowText(Win32Application::GetHwnd(), windowText.c_str());

}
