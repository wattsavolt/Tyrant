#pragma once

#include "AudioMacros.h"
#include "AudioTypes.h"

namespace tyr
{
	class AudioBackend;

	// Everything the engine and game can do with sound. Sounds are events named by path, such as
	// "event:/Player/Footstep", authored in banks. Without an audio backend, such as on a build
	// without the audio SDK, every call does nothing.
	class TYR_AUDIO_API AudioAPI final
	{
	public:
		explicit AudioAPI(AudioBackend& backend);

		// False when sound can't play, so callers can skip work only done for it.
		bool IsAvailable() const;

		// Loads a bank file. An invalid handle means it couldn't be loaded.
		AudioBankHandle LoadBank(const char* filePath);
		void UnloadBank(AudioBankHandle bank);

		// Plays the event once at the position and forgets about it. Volume scales how loud it was
		// authored, from 0 silent to 1 as authored.
		void PlayOneShot(const char* eventPath, const Vector3& position, float volume = 1.0f);

		// Starts the event at the position, to be moved, changed and stopped with the handle.
		AudioEventHandle StartEvent(const char* eventPath, const Vector3& position);
		void SetEventPosition(AudioEventHandle event, const Vector3& position);
		void SetEventParameter(AudioEventHandle event, const char* name, float value);
		// Stopping lets the event fade out the way it was authored to, unless immediate is set.
		void StopEvent(AudioEventHandle event, bool immediate = false);

		// A parameter every event can read, such as how tense the game is.
		void SetGlobalParameter(const char* name, float value);

		void SetListener(const AudioListener& listener);

		// Holds every sound where it is, such as while the game is paused, and lets them carry on.
		void SetPaused(bool paused);

	private:
		AudioBackend& m_Backend;
	};
}
