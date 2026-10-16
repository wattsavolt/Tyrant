#pragma once

#include "AudioTypes.h"

namespace tyr
{
	// What plays sound for AudioAPI. Each third-party audio library is one of these, kept inside
	// this library. The base plays nothing, for builds without one.
	class AudioBackend
	{
	public:
		virtual ~AudioBackend() = default;

		virtual bool Initialize() { return true; }
		virtual void Shutdown() {}
		virtual void Update() {}

		virtual bool IsAvailable() const { return false; }

		virtual AudioBankHandle LoadBank(const char* /*filePath*/) { return {}; }
		virtual void UnloadBank(AudioBankHandle /*bank*/) {}

		virtual void PlayOneShot(const char* /*eventPath*/, const Vector3& /*position*/, float /*volume*/) {}

		virtual AudioEventHandle StartEvent(const char* /*eventPath*/, const Vector3& /*position*/) { return {}; }
		virtual void SetEventPosition(AudioEventHandle /*event*/, const Vector3& /*position*/) {}
		virtual void SetEventParameter(AudioEventHandle /*event*/, const char* /*name*/, float /*value*/) {}
		virtual void StopEvent(AudioEventHandle /*event*/, bool /*immediate*/) {}

		virtual void SetGlobalParameter(const char* /*name*/, float /*value*/) {}

		virtual void SetListener(const AudioListener& /*listener*/) {}
		virtual void SetPaused(bool /*paused*/) {}
	};
}
