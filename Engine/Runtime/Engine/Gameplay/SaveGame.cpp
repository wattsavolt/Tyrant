#include "SaveGame.h"
#include "World/World.h"
#include "World/WorldManager.h"
#include "ECS/Components.h"
#include "Level/LevelFile.h"
#include "IO/FileStream.h"
#include "Platform/Platform.h"
#include <algorithm>
#include <cstdio>
#include <cstring>

namespace tyr
{
	namespace
	{
		constexpr uint MakeFourCC(char a, char b, char c, char d)
		{
			return static_cast<uint>(a) | (static_cast<uint>(b) << 8) | (static_cast<uint>(c) << 16) | (static_cast<uint>(d) << 24);
		}

		constexpr uint c_Magic = MakeFourCC('T', 'S', 'A', 'V');
		constexpr uint c_FormatVersion = 2;

		// What changes about an actor while it's played, as far as a save keeps it.
		enum SavedFlags : uint16
		{
			c_DoorBroken = 1 << 0,
			c_SwitchedOn = 1 << 1,
			c_FixtureOn = 1 << 2,
			c_Damaged = 1 << 3,
			c_Moves = 1 << 4,
			c_Triggered = 1 << 5
		};

		struct SavedActor
		{
			uint64 id = 0;
			float position[3] = {};
			float rotation[4] = {};
			float doorOpen = 0.0f;
			float doorSwingSign = 1.0f;
			float doorFallSign = 1.0f;
			float health = 0.0f;
			uint8 doorLock = 0;
			uint16 flags = 0;
		};

		// Reused between saves and loads, so neither allocates once it's grown to fit.
		Array<uint8> s_Buffer;
		Array<SavedActor> s_Actors;

		void PutBytes(const void* data, size_t size)
		{
			const uint start = s_Buffer.Size();
			s_Buffer.Resize(Array<uint8>::UninitializedTag{}, start + static_cast<uint>(size));
			memcpy(s_Buffer.Data() + start, data, size);
		}

		template<typename T>
		void Put(const T& value)
		{
			PutBytes(&value, sizeof(T));
		}

		class Reader final
		{
		public:
			Reader(const uint8* data, size_t size)
				: m_Data(data)
				, m_Size(size)
			{
			}

			bool ReadBytes(void* out, size_t size)
			{
				if (m_Failed || size > m_Size - m_Offset)
				{
					m_Failed = true;
					return false;
				}
				memcpy(out, m_Data + m_Offset, size);
				m_Offset += size;
				return true;
			}

			template<typename T>
			T Read()
			{
				T value{};
				ReadBytes(&value, sizeof(T));
				return value;
			}

			bool Failed() const { return m_Failed; }

		private:
			const uint8* m_Data;
			size_t m_Size;
			size_t m_Offset = 0;
			bool m_Failed = false;
		};

		// Finds the first entity of the actor with the component, or c_InvalidEntity.
		template<typename T>
		Entity FindWith(const World& world, const ActorInstance& actor)
		{
			for (Entity entity : actor.entities)
			{
				if (world.entities.HasComponent<T>(entity))
				{
					return entity;
				}
			}
			return c_InvalidEntity;
		}

		SavedActor Capture(const World& world, const ActorInstance& actor)
		{
			SavedActor saved;
			saved.id = actor.id.GetHash();
			const Entity root = actor.RootEntity();
			// Only things that move about by themselves are put back where they were. Doors are put back by how open they are.
			const bool moves = world.entities.HasComponent<CharacterComponent>(root) || world.entities.HasComponent<RigidBodyComponent>(root);
			if (moves && world.entities.HasComponent<TransformComponent>(root))
			{
				const Transform& transform = world.entities.GetComponent<TransformComponent>(root).world;
				saved.position[0] = transform.position.x;
				saved.position[1] = transform.position.y;
				saved.position[2] = transform.position.z;
				saved.rotation[0] = transform.rotation.x;
				saved.rotation[1] = transform.rotation.y;
				saved.rotation[2] = transform.rotation.z;
				saved.rotation[3] = transform.rotation.w;
				saved.flags |= c_Moves;
			}
			const Entity door = FindWith<DoorComponent>(world, actor);
			if (door != c_InvalidEntity)
			{
				const DoorComponent& component = world.entities.GetComponent<DoorComponent>(door);
				saved.doorLock = component.lock;
				saved.doorOpen = component.targetAmount;
				saved.doorSwingSign = component.swingSign;
				saved.doorFallSign = component.fallSign;
				saved.flags |= component.broken ? c_DoorBroken : 0;
			}
			const Entity lightSwitch = FindWith<LightSwitchComponent>(world, actor);
			if (lightSwitch != c_InvalidEntity && world.entities.GetComponent<LightSwitchComponent>(lightSwitch).switchedOn)
			{
				saved.flags |= c_SwitchedOn;
			}
			const Entity fixture = FindWith<LightFixtureComponent>(world, actor);
			if (fixture != c_InvalidEntity && world.entities.GetComponent<LightFixtureComponent>(fixture).switchedOn)
			{
				saved.flags |= c_FixtureOn;
			}
			const Entity trigger = FindWith<TriggerVolumeComponent>(world, actor);
			if (trigger != c_InvalidEntity && world.entities.GetComponent<TriggerVolumeComponent>(trigger).triggered)
			{
				saved.flags |= c_Triggered;
			}
			const Entity health = FindWith<HealthComponent>(world, actor);
			if (health != c_InvalidEntity)
			{
				const HealthComponent& component = world.entities.GetComponent<HealthComponent>(health);
				saved.health = component.health;
				saved.flags |= component.damaged ? c_Damaged : 0;
			}
			return saved;
		}

		void Restore(WorldManager& worldManager, Handle worldHandle, const ActorInstance& actor, const SavedActor& saved)
		{
			World& world = worldManager.GetWorld(worldHandle);
			if (saved.flags & c_Moves)
			{
				Transform transform = world.entities.GetComponent<TransformComponent>(actor.RootEntity()).world;
				transform.position = Vector3(saved.position[0], saved.position[1], saved.position[2]);
				transform.rotation = Quaternion(saved.rotation[0], saved.rotation[1], saved.rotation[2], saved.rotation[3]);
				worldManager.SetActorTransform(worldHandle, actor.RootEntity(), transform);
			}
			// Doors swing open, or fall, from where they were placed, while the save fades in.
			const Entity door = FindWith<DoorComponent>(world, actor);
			if (door != c_InvalidEntity)
			{
				DoorComponent& component = world.entities.GetComponent<DoorComponent>(door);
				component.lock = saved.doorLock;
				component.targetAmount = saved.doorOpen;
				component.swingSign = saved.doorSwingSign;
				component.broken = (saved.flags & c_DoorBroken) != 0;
				component.fallSign = saved.doorFallSign;
				component.fallAmount = 0.0f;
				component.targetAmount = component.broken ? 0.0f : component.targetAmount;
			}
			const Entity lightSwitch = FindWith<LightSwitchComponent>(world, actor);
			if (lightSwitch != c_InvalidEntity)
			{
				world.entities.GetComponent<LightSwitchComponent>(lightSwitch).switchedOn = (saved.flags & c_SwitchedOn) != 0;
			}
			const Entity fixture = FindWith<LightFixtureComponent>(world, actor);
			if (fixture != c_InvalidEntity)
			{
				world.entities.GetComponent<LightFixtureComponent>(fixture).switchedOn = (saved.flags & c_FixtureOn) != 0;
			}
			const Entity trigger = FindWith<TriggerVolumeComponent>(world, actor);
			if (trigger != c_InvalidEntity)
			{
				world.entities.GetComponent<TriggerVolumeComponent>(trigger).triggered = (saved.flags & c_Triggered) != 0;
			}
			const Entity health = FindWith<HealthComponent>(world, actor);
			if (health != c_InvalidEntity)
			{
				HealthComponent& component = world.entities.GetComponent<HealthComponent>(health);
				component.health = saved.health;
				component.damaged = (saved.flags & c_Damaged) != 0;
			}
		}
	}

	namespace SaveGame
	{
		bool Write(const char* absPath, const World& world, const char* levelPath, const void* gameData, uint gameDataSize)
		{
			if (gameDataSize > c_MaxGameData)
			{
				TYR_LOG_ERROR("The game's save data is too big, at %u bytes.", gameDataSize);
				return false;
			}
			s_Buffer.Clear();
			s_Buffer.Reserve(64 + static_cast<uint>(strlen(levelPath)) + world.actorInstances.Size() * sizeof(SavedActor) + gameDataSize);
			Put(c_Magic);
			Put(c_FormatVersion);
			const uint16 levelLength = static_cast<uint16>(strlen(levelPath));
			Put(levelLength);
			PutBytes(levelPath, levelLength);
			Put(world.actorInstances.Size());
			for (const ActorInstance& actor : world.actorInstances)
			{
				Put(Capture(world, actor));
			}
			Put(gameDataSize);
			PutBytes(gameData, gameDataSize);

			// Written beside it first, so a save that fails part way doesn't lose the last one.
			char tempPath[TYR_MAX_PATH_TOTAL_SIZE];
			snprintf(tempPath, sizeof(tempPath), "%s.tmp", absPath);
			FileStream::WriteFile(tempPath, s_Buffer.Data(), s_Buffer.Size());
			if (FileStream::GetFileSize(tempPath) != s_Buffer.Size())
			{
				TYR_LOG_ERROR("Couldn't write the save %s.", absPath);
				return false;
			}
			return Platform::RenameFile(tempPath, absPath);
		}

		bool Load(const char* absPath, WorldManager& worldManager, Handle worldHandle, uint8* gameData, uint gameDataCapacity,
			uint& outGameDataSize, RelativePath& outLevelPath)
		{
			outGameDataSize = 0;
			if (!Platform::FileExists(absPath))
			{
				return false;
			}
			const size_t size = FileStream::ReadAllFile(absPath, s_Buffer);
			Reader reader(s_Buffer.Data(), size);
			if (reader.Read<uint>() != c_Magic || reader.Read<uint>() != c_FormatVersion)
			{
				TYR_LOG_ERROR("%s isn't a save this version can load.", absPath);
				return false;
			}
			char levelPath[RelativePath::c_Capacity + 1] = {};
			const uint16 levelLength = reader.Read<uint16>();
			if (levelLength > RelativePath::c_Capacity || !reader.ReadBytes(levelPath, levelLength))
			{
				return false;
			}
			const uint actorCount = reader.Read<uint>();
			s_Actors.Clear();
			for (uint i = 0; i < actorCount && !reader.Failed(); ++i)
			{
				s_Actors.Add(reader.Read<SavedActor>());
			}
			const uint dataSize = reader.Read<uint>();
			if (reader.Failed() || dataSize > gameDataCapacity || !reader.ReadBytes(gameData, dataSize))
			{
				TYR_LOG_ERROR("The save %s is damaged.", absPath);
				return false;
			}

			if (!LevelFile::Load(levelPath, worldManager, worldHandle))
			{
				return false;
			}
			outLevelPath = levelPath;
			outGameDataSize = dataSize;

			// Level actors the save doesn't have are gone, such as items that were taken.
			std::sort(s_Actors.begin(), s_Actors.end(), [](const SavedActor& a, const SavedActor& b) { return a.id < b.id; });
			World& world = worldManager.GetWorld(worldHandle);
			for (uint i = world.actorInstances.Size(); i-- > 0;)
			{
				const ActorInstance& actor = world.actorInstances[i];
				const uint64 id = actor.id.GetHash();
				const SavedActor* found = std::lower_bound(s_Actors.begin(), s_Actors.end(), id,
					[](const SavedActor& saved, uint64 value) { return saved.id < value; });
				if (found == s_Actors.end() || found->id != id)
				{
					worldManager.RemoveActorInstance(worldHandle, actor.RootEntity());
					continue;
				}
				Restore(worldManager, worldHandle, actor, *found);
			}
			return true;
		}
	}
}
