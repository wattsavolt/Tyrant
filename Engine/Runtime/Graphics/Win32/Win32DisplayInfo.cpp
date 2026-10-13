#include "RenderAPI/DisplayInfo.h"
#include <dxgi1_6.h>

namespace tyr
{
	namespace
	{
		// Kept between queries, since it goes stale when the display setup changes.
		IDXGIFactory1* s_Factory = nullptr;

		IDXGIFactory1* GetFactory()
		{
			if (s_Factory && !s_Factory->IsCurrent())
			{
				s_Factory->Release();
				s_Factory = nullptr;
			}
			if (!s_Factory && FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&s_Factory))))
			{
				s_Factory = nullptr;
			}
			return s_Factory;
		}

		// Reads the HDR state of the output showing monitor. False if it wasn't on this adapter.
		bool QueryAdapterOutputs(IDXGIAdapter1* adapter, HMONITOR monitor, DisplayHDRInfo& outInfo)
		{
			IDXGIOutput* output = nullptr;
			for (UINT outputIndex = 0; adapter->EnumOutputs(outputIndex, &output) != DXGI_ERROR_NOT_FOUND; ++outputIndex)
			{
				DXGI_OUTPUT_DESC desc;
				const bool found = SUCCEEDED(output->GetDesc(&desc)) && desc.Monitor == monitor;
				IDXGIOutput6* output6 = nullptr;
				if (found && SUCCEEDED(output->QueryInterface(__uuidof(IDXGIOutput6), reinterpret_cast<void**>(&output6))))
				{
					DXGI_OUTPUT_DESC1 desc1;
					if (SUCCEEDED(output6->GetDesc1(&desc1)))
					{
						outInfo.osHdrEnabled = desc1.ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020;
						outInfo.minLuminance = desc1.MinLuminance;
						outInfo.maxLuminance = desc1.MaxLuminance;
						outInfo.maxFullFrameLuminance = desc1.MaxFullFrameLuminance;
					}
					output6->Release();
				}
				output->Release();
				if (found)
				{
					return true;
				}
			}
			return false;
		}
	}

	void DisplayUtil::QueryDisplay(void* windowOSHandle, DisplayHDRInfo& outInfo)
	{
		outInfo = {};
		const HMONITOR monitor = MonitorFromWindow(static_cast<HWND>(windowOSHandle), MONITOR_DEFAULTTONEAREST);
		outInfo.monitor = monitor;

		IDXGIFactory1* factory = GetFactory();
		if (!factory)
		{
			return;
		}

		IDXGIAdapter1* adapter = nullptr;
		for (UINT adapterIndex = 0; factory->EnumAdapters1(adapterIndex, &adapter) != DXGI_ERROR_NOT_FOUND; ++adapterIndex)
		{
			const bool found = QueryAdapterOutputs(adapter, monitor, outInfo);
			adapter->Release();
			if (found)
			{
				return;
			}
		}
	}

	bool DisplayUtil::HasDisplayChanged(void* windowOSHandle, const DisplayHDRInfo& info)
	{
		const HMONITOR monitor = MonitorFromWindow(static_cast<HWND>(windowOSHandle), MONITOR_DEFAULTTONEAREST);
		return monitor != info.monitor || (s_Factory && !s_Factory->IsCurrent());
	}

	void DisplayUtil::Shutdown()
	{
		if (s_Factory)
		{
			s_Factory->Release();
			s_Factory = nullptr;
		}
	}
}
