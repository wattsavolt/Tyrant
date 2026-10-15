#include "FMODAudioBackend.h"
#include "fmod_studio.hpp"
#include "fmod_errors.h"

namespace tyr
{
	namespace
	{
		// Sounds FMOD mixes at once. Quieter ones are virtualised past this.
		constexpr int c_MaxChannels = 512;

		bool Check(FMOD_RESULT result, const char* what)
		{
			if (result != FMOD_OK)
			{
				TYR_LOG_ERROR("FMOD couldn't %s: %s", what, FMOD_ErrorString(result));
				return false;
			}
			return true;
		}

		FMOD_VECTOR ToFMOD(const Vector3& v)
		{
			return { v.x, v.y, v.z };
		}

		// FMOD is left-handed by default, the same as the engine, so vectors go across as they are.
		FMOD_3D_ATTRIBUTES MakeAttributes(const Vector3& position)
		{
			FMOD_3D_ATTRIBUTES attributes = {};
			attributes.position = ToFMOD(position);
			attributes.forward = { 0.0f, 0.0f, 1.0f };
			attributes.up = { 0.0f, 1.0f, 0.0f };
			return attributes;
		}
	}

	bool FMODAudioBackend::Initialize()
	{
		if (!Check(FMOD::Studio::System::create(&m_System), "create its system"))
		{
			m_System = nullptr;
			return false;
		}
		if (!Check(m_System->initialize(c_MaxChannels, FMOD_STUDIO_INIT_NORMAL, FMOD_INIT_NORMAL, nullptr), "start"))
		{
			m_System->release();
			m_System = nullptr;
			return false;
		}
		return true;
	}

	void FMODAudioBackend::Shutdown()
	{
		if (m_System)
		{
			// Releasing the system releases its banks and event instances too.
			m_System->release();
			m_System = nullptr;
		}
		for (FMOD::Studio::Bank*& bank : m_Banks)
		{
			bank = nullptr;
		}
		for (EventSlot& slot : m_Events)
		{
			slot.instance = nullptr;
		}
	}

	void FMODAudioBackend::Update()
	{
		if (m_System)
		{
			Check(m_System->update(), "update");
		}
	}

	AudioBankHandle FMODAudioBackend::LoadBank(const char* filePath)
	{
		for (uint i = 0; i < AudioConstants::c_MaxBanks; ++i)
		{
			if (m_Banks[i])
			{
				continue;
			}
			if (!Check(m_System->loadBankFile(filePath, FMOD_STUDIO_LOAD_BANK_NORMAL, &m_Banks[i]), "load a bank"))
			{
				TYR_LOG_ERROR("The bank was %s.", filePath);
				m_Banks[i] = nullptr;
				return {};
			}
			return { i + 1 };
		}
		TYR_LOG_ERROR("Can't load %s, there are already %u banks loaded.", filePath, AudioConstants::c_MaxBanks);
		return {};
	}

	void FMODAudioBackend::UnloadBank(AudioBankHandle bank)
	{
		if (bank.IsValid() && bank.id <= AudioConstants::c_MaxBanks && m_Banks[bank.id - 1])
		{
			m_Banks[bank.id - 1]->unload();
			m_Banks[bank.id - 1] = nullptr;
		}
	}

	FMOD::Studio::EventInstance* FMODAudioBackend::CreateInstance(const char* eventPath, const Vector3& position)
	{
		FMOD::Studio::EventDescription* description = nullptr;
		if (!m_System || !Check(m_System->getEvent(eventPath, &description), "find a sound event"))
		{
			TYR_LOG_ERROR("The event was %s.", eventPath ? eventPath : "");
			return nullptr;
		}
		FMOD::Studio::EventInstance* instance = nullptr;
		if (!Check(description->createInstance(&instance), "make a sound event"))
		{
			return nullptr;
		}
		const FMOD_3D_ATTRIBUTES attributes = MakeAttributes(position);
		instance->set3DAttributes(&attributes);
		return instance;
	}

	void FMODAudioBackend::PlayOneShot(const char* eventPath, const Vector3& position)
	{
		FMOD::Studio::EventInstance* instance = CreateInstance(eventPath, position);
		if (instance)
		{
			// Released now, FMOD frees it once it's finished playing.
			instance->start();
			instance->release();
		}
	}

	AudioEventHandle FMODAudioBackend::StartEvent(const char* eventPath, const Vector3& position)
	{
		uint free = AudioConstants::c_MaxEvents;
		for (uint i = 0; i < AudioConstants::c_MaxEvents; ++i)
		{
			// A slot whose event has stopped and been released by FMOD is free again.
			if (!m_Events[i].instance || !m_Events[i].instance->isValid())
			{
				m_Events[i].instance = nullptr;
				free = i;
				break;
			}
		}
		if (free == AudioConstants::c_MaxEvents)
		{
			TYR_LOG_ERROR("Can't start %s, there are already %u sound events playing.", eventPath, AudioConstants::c_MaxEvents);
			return {};
		}

		FMOD::Studio::EventInstance* instance = CreateInstance(eventPath, position);
		if (!instance)
		{
			return {};
		}
		instance->start();
		EventSlot& slot = m_Events[free];
		slot.instance = instance;
		slot.generation = slot.generation + 1 == 0 ? 1 : slot.generation + 1;
		return { free, slot.generation };
	}

	FMOD::Studio::EventInstance* FMODAudioBackend::GetInstance(AudioEventHandle event) const
	{
		if (!event.IsValid() || event.index >= AudioConstants::c_MaxEvents)
		{
			return nullptr;
		}
		const EventSlot& slot = m_Events[event.index];
		return slot.generation == event.generation && slot.instance && slot.instance->isValid() ? slot.instance : nullptr;
	}

	void FMODAudioBackend::SetEventPosition(AudioEventHandle event, const Vector3& position)
	{
		if (FMOD::Studio::EventInstance* instance = GetInstance(event))
		{
			const FMOD_3D_ATTRIBUTES attributes = MakeAttributes(position);
			instance->set3DAttributes(&attributes);
		}
	}

	void FMODAudioBackend::SetEventParameter(AudioEventHandle event, const char* name, float value)
	{
		if (FMOD::Studio::EventInstance* instance = GetInstance(event))
		{
			Check(instance->setParameterByName(name, value), "set a sound event's parameter");
		}
	}

	void FMODAudioBackend::StopEvent(AudioEventHandle event, bool immediate)
	{
		if (FMOD::Studio::EventInstance* instance = GetInstance(event))
		{
			instance->stop(immediate ? FMOD_STUDIO_STOP_IMMEDIATE : FMOD_STUDIO_STOP_ALLOWFADEOUT);
			instance->release();
			m_Events[event.index].instance = nullptr;
		}
	}

	void FMODAudioBackend::SetGlobalParameter(const char* name, float value)
	{
		if (m_System)
		{
			Check(m_System->setParameterByName(name, value), "set a global parameter");
		}
	}

	void FMODAudioBackend::SetListener(const AudioListener& listener)
	{
		if (!m_System)
		{
			return;
		}
		FMOD_3D_ATTRIBUTES attributes = {};
		attributes.position = ToFMOD(listener.position);
		attributes.velocity = ToFMOD(listener.velocity);
		attributes.forward = ToFMOD(listener.forward);
		attributes.up = ToFMOD(listener.up);
		m_System->setListenerAttributes(0, &attributes);
	}
}
