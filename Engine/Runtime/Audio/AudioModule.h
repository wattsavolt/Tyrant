#pragma once

#include "Core.h"
#include "AudioMacros.h"
#include "Module/IModule.h"

namespace tyr
{
	class AudioAPI;
	class AudioBackend;

	// Starts the audio backend and updates it every frame. Without one built in, sound is off
	// and AudioAPI calls do nothing.
	class TYR_AUDIO_API AudioModule final : public IModule
	{
	public:
		AudioModule();
		~AudioModule();

		void Initialize() override;
		void Shutdown() override;
		void Update(float deltaTime) override;

		AudioAPI* GetAudioAPI() const { return m_AudioAPI; }

	private:
		AudioBackend* m_Backend{};
		AudioAPI* m_AudioAPI{};
	};
}
