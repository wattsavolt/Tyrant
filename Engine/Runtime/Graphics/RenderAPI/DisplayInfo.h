#pragma once

#include "GraphicsBase.h"

namespace tyr
{
	// What the display showing a window can do, for choosing between HDR and SDR output.
	struct DisplayHDRInfo
	{
		// The OS's handle to the display the window is mostly on.
		void* monitor = nullptr;
		// The OS is showing that display in HDR.
		bool osHdrEnabled = false;
		// The OS shows the display in HDR and the device can present HDR10 to the window.
		bool hdrSupported = false;
		// In nits, as the display reports them. Zero when it doesn't say.
		float minLuminance = 0.0f;
		float maxLuminance = 0.0f;
		float maxFullFrameLuminance = 0.0f;
	};

	// Reads display state from the OS. Main thread only.
	namespace DisplayUtil
	{
		// Fills in everything but hdrSupported, which needs the device.
		void QueryDisplay(void* windowOSHandle, DisplayHDRInfo& outInfo);
		// True when the window is on another display, or the OS's display setup has changed,
		// such as HDR being turned on or off, since info was queried.
		bool HasDisplayChanged(void* windowOSHandle, const DisplayHDRInfo& info);
		void Shutdown();
	}
}
