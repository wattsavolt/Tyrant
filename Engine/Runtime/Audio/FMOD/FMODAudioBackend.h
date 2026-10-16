#pragma once

#include "Private/AudioBackend.h"

namespace FMOD
{
	namespace Studio
	{
		class System;
		class Bank;
		class EventInstance;
	}
}

namespace tyr
{
	// Plays FMOD Studio banks and events. Only built when the FMOD SDK is there.
	class FMODAudioBackend final : public AudioBackend
	{
	public:
		bool Initialize() override;
		void Shutdown() override;
		void Update() override;

		bool IsAvailable() const override { return m_System != nullptr; }

		AudioBankHandle LoadBank(const char* filePath) override;
		void UnloadBank(AudioBankHandle bank) override;

		void PlayOneShot(const char* eventPath, const Vector3& position, float volume) override;

		AudioEventHandle StartEvent(const char* eventPath, const Vector3& position) override;
		void SetEventPosition(AudioEventHandle event, const Vector3& position) override;
		void SetEventParameter(AudioEventHandle event, const char* name, float value) override;
		void StopEvent(AudioEventHandle event, bool immediate) override;

		void SetGlobalParameter(const char* name, float value) override;

		void SetListener(const AudioListener& listener) override;
		void SetPaused(bool paused) override;

	private:
		// Makes an instance of the event placed at the position, or null if there's no such event.
		FMOD::Studio::EventInstance* CreateInstance(const char* eventPath, const Vector3& position);
		// The instance the handle names, or null once it's stopped or its slot is reused.
		FMOD::Studio::EventInstance* GetInstance(AudioEventHandle event) const;

		struct EventSlot
		{
			FMOD::Studio::EventInstance* instance = nullptr;
			// Goes up each time the slot is used, so old handles stop matching. Never 0 once used.
			uint generation = 0;
		};

		FMOD::Studio::System* m_System = nullptr;
		FMOD::Studio::Bank* m_Banks[AudioConstants::c_MaxBanks] = {};
		EventSlot m_Events[AudioConstants::c_MaxEvents];
	};
}
