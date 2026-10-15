#include "AudioModule.h"
#include "AudioAPI.h"
#include "Private/AudioBackend.h"
#if TYR_HAS_FMOD
#include "FMOD/FMODAudioBackend.h"
#endif

namespace tyr
{
	AudioModule::AudioModule()
	{
	}

	AudioModule::~AudioModule()
	{
	}

	void AudioModule::Initialize()
	{
#if TYR_HAS_FMOD
		m_Backend = new FMODAudioBackend();
#else
		m_Backend = new AudioBackend();
#endif
		// Without a working device the game still runs, silently.
		if (!m_Backend->Initialize())
		{
			TYR_LOG_WARNING("Audio couldn't start, so there's no sound.");
			TYR_SAFE_DELETE(m_Backend);
			m_Backend = new AudioBackend();
		}
		else if (!m_Backend->IsAvailable())
		{
			TYR_LOG_INFO("Built without an audio library, so there's no sound.");
		}
		m_AudioAPI = new AudioAPI(*m_Backend);
	}

	void AudioModule::Update(float /*deltaTime*/)
	{
		m_Backend->Update();
	}

	void AudioModule::Shutdown()
	{
		TYR_SAFE_DELETE(m_AudioAPI);
		m_Backend->Shutdown();
		TYR_SAFE_DELETE(m_Backend);
	}
}
