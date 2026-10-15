#include "AudioAPI.h"
#include "Private/AudioBackend.h"

namespace tyr
{
	AudioAPI::AudioAPI(AudioBackend& backend)
		: m_Backend(backend)
	{
	}

	bool AudioAPI::IsAvailable() const
	{
		return m_Backend.IsAvailable();
	}

	AudioBankHandle AudioAPI::LoadBank(const char* filePath)
	{
		return m_Backend.LoadBank(filePath);
	}

	void AudioAPI::UnloadBank(AudioBankHandle bank)
	{
		m_Backend.UnloadBank(bank);
	}

	void AudioAPI::PlayOneShot(const char* eventPath, const Vector3& position)
	{
		m_Backend.PlayOneShot(eventPath, position);
	}

	AudioEventHandle AudioAPI::StartEvent(const char* eventPath, const Vector3& position)
	{
		return m_Backend.StartEvent(eventPath, position);
	}

	void AudioAPI::SetEventPosition(AudioEventHandle event, const Vector3& position)
	{
		m_Backend.SetEventPosition(event, position);
	}

	void AudioAPI::SetEventParameter(AudioEventHandle event, const char* name, float value)
	{
		m_Backend.SetEventParameter(event, name, value);
	}

	void AudioAPI::StopEvent(AudioEventHandle event, bool immediate)
	{
		m_Backend.StopEvent(event, immediate);
	}

	void AudioAPI::SetGlobalParameter(const char* name, float value)
	{
		m_Backend.SetGlobalParameter(name, value);
	}

	void AudioAPI::SetListener(const AudioListener& listener)
	{
		m_Backend.SetListener(listener);
	}
}
