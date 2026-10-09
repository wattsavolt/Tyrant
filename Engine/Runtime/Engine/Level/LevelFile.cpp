#include "LevelFile.h"
#include "OverridableFields.h"
#include "World/World.h"
#include "World/WorldManager.h"
#include "Actor/ActorRegistry.h"
#include "ECS/Components.h"
#include "AssetSystem/AssetRegistry.h"
#include "AssetSystem/AssetUtil.h"
#include "Reflection/TypeRegistry.h"
#include "Reflection/ReflectionUtil.h"
#include "Reflection/CustomObjectPropertiesReflector.h"
#include "IO/FileStream.h"
#include "Utility/PathUtil.h"
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

		constexpr uint c_Magic = MakeFourCC('T', 'L', 'V', 'L');
		constexpr uint c_FormatVersion = 1;
		constexpr uint c_SettingsSection = MakeFourCC('S', 'E', 'T', 'T');
		constexpr uint c_FoldersSection = MakeFourCC('F', 'O', 'L', 'D');
		constexpr uint c_DependenciesSection = MakeFourCC('D', 'E', 'P', 'S');
		constexpr uint c_ActorsSection = MakeFourCC('A', 'C', 'T', 'R');
		constexpr uint16 c_RootFolder = 0xFFFF;

		// Writes into a reused buffer, so the whole file can be written in one go.
		class ByteWriter final
		{
		public:
			explicit ByteWriter(Array<uint8>& buffer)
				: m_Buffer(buffer)
			{
				m_Buffer.Clear();
			}

			void WriteBytes(const void* data, size_t size)
			{
				const uint offset = m_Buffer.Size();
				m_Buffer.Resize(Array<uint8>::UninitializedTag{}, offset + static_cast<uint>(size));
				memcpy(m_Buffer.Data() + offset, data, size);
			}

			template<typename T>
			void Write(const T& value)
			{
				WriteBytes(&value, sizeof(T));
			}

			void WriteString(const char* string)
			{
				const uint16 length = static_cast<uint16>(strlen(string));
				Write(length);
				WriteBytes(string, length);
			}

			// Writes a placeholder to fill in with Patch once the value is known.
			uint WritePlaceholder()
			{
				const uint offset = m_Buffer.Size();
				Write<uint>(0);
				return offset;
			}

			void Patch(uint offset, uint value)
			{
				memcpy(m_Buffer.Data() + offset, &value, sizeof(value));
			}

			uint GetOffset() const { return m_Buffer.Size(); }

		private:
			Array<uint8>& m_Buffer;
		};

		// Reads from a buffer, failing rather than reading past its end.
		class ByteReader final
		{
		public:
			ByteReader(const uint8* data, size_t size)
				: m_Data(data)
				, m_Size(size)
			{
			}

			// Null, and failed from then on, if there aren't enough bytes left.
			const uint8* ReadSpan(size_t size)
			{
				if (m_Failed || size > m_Size - m_Offset)
				{
					m_Failed = true;
					return nullptr;
				}
				const uint8* span = m_Data + m_Offset;
				m_Offset += size;
				return span;
			}

			template<typename T>
			T Read()
			{
				T value{};
				if (const uint8* span = ReadSpan(sizeof(T)))
				{
					memcpy(&value, span, sizeof(T));
				}
				return value;
			}

			// Cut short to fit the string's capacity.
			template<uint N>
			void ReadString(LocalString<N>& string)
			{
				const uint16 length = Read<uint16>();
				const char* chars = reinterpret_cast<const char*>(ReadSpan(length));
				string = chars ? LocalString<N>(chars, std::min<size_t>(length, N)) : LocalString<N>();
			}

			size_t GetRemaining() const { return m_Size - m_Offset; }
			bool Failed() const { return m_Failed; }

		private:
			const uint8* m_Data;
			size_t m_Size;
			size_t m_Offset = 0;
			bool m_Failed = false;
		};

		Transform CreateIdentityTransform()
		{
			Transform transform;
			transform.position = Vector3::c_Zero;
			transform.rotation = Quaternion::c_Identity;
			transform.scale = Vector3::c_One;
			return transform;
		}

		// One value per statement, since the order of function arguments isn't defined.
		void ReadTransform(ByteReader& reader, Transform& transform)
		{
			transform.position.x = reader.Read<float>();
			transform.position.y = reader.Read<float>();
			transform.position.z = reader.Read<float>();
			transform.rotation.x = reader.Read<float>();
			transform.rotation.y = reader.Read<float>();
			transform.rotation.z = reader.Read<float>();
			transform.rotation.w = reader.Read<float>();
			transform.scale.x = reader.Read<float>();
			transform.scale.y = reader.Read<float>();
			transform.scale.z = reader.Read<float>();
		}

		// Copies one saved value into the object, if its key still exists with the same size.
		bool ApplyTaggedValue(const OverridableFields::FieldList& fields, const Id64& key, const uint8* value, uint size, uint8* object)
		{
			const OverridableField* field = OverridableFields::Find(fields, key);
			if (!field || field->size != size || !value)
			{
				return false;
			}
			memcpy(object + field->offset, value, size);
			return true;
		}

		void ReadSettings(ByteReader& reader, LevelSettings& settings)
		{
			const OverridableFields::FieldList& fields = OverridableFields::Get(GetTypeID<LevelSettings>());
			const uint count = reader.Read<uint>();
			for (uint i = 0; i < count && !reader.Failed(); ++i)
			{
				const Id64 key = reader.Read<uint64>();
				const uint size = reader.Read<uint>();
				ApplyTaggedValue(fields, key, reader.ReadSpan(size), size, reinterpret_cast<uint8*>(&settings));
			}
		}

		void ReadFolders(ByteReader& reader, Array<RelativePath>& folders)
		{
			const uint count = reader.Read<uint>();
			folders.Clear();
			for (uint i = 0; i < count && !reader.Failed(); ++i)
			{
				reader.ReadString(folders.ExpandOne());
			}
		}

		// Applies one override to the actor's newly made entities. False if it no longer matches the actor type.
		bool ApplyOverride(EntitySystem& entities, const ActorTypeDesc& actorType, const LocalArray<Entity, c_MaxActorTypeEntities>& actorEntities,
			const Id64& entityName, const Id64& componentType, const Id64& key, const uint8* value, uint size)
		{
			Entity entity = c_InvalidEntity;
			for (uint i = 0; i < actorType.entities.Size() && i < actorEntities.Size(); ++i)
			{
				if (actorType.entities[i].localNameId == entityName)
				{
					entity = actorEntities[i];
					break;
				}
			}

			ComponentTypeID componentTypeID;
			if (entity == c_InvalidEntity
				|| !ComponentRegistry::Instance().FindComponentTypeID(componentType, componentTypeID)
				|| !entities.HasComponent(entity, componentTypeID))
			{
				return false;
			}

			uint8* component = static_cast<uint8*>(entities.GetComponentData(entity, componentTypeID));
			return ApplyTaggedValue(OverridableFields::Get(componentType), key, value, size, component);
		}

		void ReadActors(ByteReader& reader, WorldManager& worldManager, Handle worldHandle, const Array<RelativePath>& folders)
		{
			static HashMap<Id64, uint8> s_LoadedIDs;
			s_LoadedIDs.Clear();

			World& world = worldManager.GetWorld(worldHandle);
			const uint count = reader.Read<uint>();
			for (uint i = 0; i < count && !reader.Failed(); ++i)
			{
				const Id64 id = reader.Read<uint64>();
				const Id64 typeID = reader.Read<uint64>();
				ActorName name;
				reader.ReadString(name);
				const uint16 folderIndex = reader.Read<uint16>();
				Transform transform;
				ReadTransform(reader, transform);
				const uint overrideCount = reader.Read<uint>();

				if (s_LoadedIDs.Contains(id))
				{
					TYR_LOG_WARNING("%s has the same ID as another actor in the level.", name.CStr());
				}
				s_LoadedIDs[id] = 1;

				const ActorTypeDesc* actorType = ActorRegistry::Instance().FindActorType(typeID);
				LocalArray<Entity, c_MaxActorTypeEntities> entities;
				if (actorType)
				{
					entities = ActorRegistry::Instance().InstantiateActor(typeID, world.entities);
				}
				else
				{
					TYR_LOG_ERROR("%s was skipped, as its actor type no longer exists.", name.CStr());
				}

				uint skipped = 0;
				for (uint j = 0; j < overrideCount && !reader.Failed(); ++j)
				{
					const Id64 entityName = reader.Read<uint64>();
					const Id64 componentType = reader.Read<uint64>();
					const Id64 key = reader.Read<uint64>();
					const uint size = reader.Read<uint>();
					const uint8* value = reader.ReadSpan(size);
					if (actorType && !ApplyOverride(world.entities, *actorType, entities, entityName, componentType, key, value, size))
					{
						++skipped;
					}
				}
				if (skipped > 0)
				{
					TYR_LOG_WARNING("%s had %u saved values that no longer match its actor type, so they were skipped.", name.CStr(), skipped);
				}

				if (!actorType || entities.IsEmpty())
				{
					continue;
				}

				const char* folder = folderIndex < folders.Size() ? folders[folderIndex].CStr() : "";
				worldManager.AddActorInstance(worldHandle, id, typeID, name.CStr(), folder, entities);
				if (world.entities.HasComponent<ComponentTransform>(entities[0]))
				{
					worldManager.SetActorTransform(worldHandle, entities[0], transform);
				}
			}
		}

#if TYR_EDITOR
		void WriteTransform(ByteWriter& writer, const Transform& transform)
		{
			writer.Write(transform.position.x);
			writer.Write(transform.position.y);
			writer.Write(transform.position.z);
			writer.Write(transform.rotation.x);
			writer.Write(transform.rotation.y);
			writer.Write(transform.rotation.z);
			writer.Write(transform.rotation.w);
			writer.Write(transform.scale.x);
			writer.Write(transform.scale.y);
			writer.Write(transform.scale.z);
		}

		uint BeginSection(ByteWriter& writer, uint sectionID)
		{
			writer.Write(sectionID);
			return writer.WritePlaceholder();
		}

		void EndSection(ByteWriter& writer, uint sizeOffset)
		{
			writer.Patch(sizeOffset, writer.GetOffset() - sizeOffset - static_cast<uint>(sizeof(uint)));
		}

		void WriteSettings(ByteWriter& writer, const LevelSettings& settings)
		{
			const OverridableFields::FieldList& fields = OverridableFields::Get(GetTypeID<LevelSettings>());
			const uint8* data = reinterpret_cast<const uint8*>(&settings);
			writer.Write(fields.Size());
			for (const OverridableField& field : fields)
			{
				writer.Write(field.key.GetHash());
				writer.Write(field.size);
				writer.WriteBytes(data + field.offset, field.size);
			}
		}

		// Every folder with something in it or made in the hierarchy, each once.
		void BuildFolders(const World& world, Array<RelativePath>& folders)
		{
			folders.Clear();
			for (const RelativePath& folder : world.folders)
			{
				if (!folders.Contains(folder))
				{
					folders.Add(folder);
				}
			}
			for (const ActorInstance& actor : world.actorInstances)
			{
				if (actor.folderPath.Size() > 0 && !folders.Contains(actor.folderPath))
				{
					folders.Add(actor.folderPath);
				}
			}
		}

		uint16 FindFolderIndex(const Array<RelativePath>& folders, const RelativePath& folder)
		{
			for (uint i = 0; i < folders.Size(); ++i)
			{
				if (folders[i] == folder)
				{
					return static_cast<uint16>(i);
				}
			}
			return c_RootFolder;
		}

		void AddDependency(AssetID assetID, Array<AssetID>& dependencies)
		{
			// Other ID fields aren't assets, so only registered assets count.
			if (AssetUtil::IsValidAssetID(assetID) && AssetRegistry::Instance().GetAssets().Contains(assetID) && !dependencies.Contains(assetID))
			{
				dependencies.Add(assetID);
			}
		}

		void CollectAssetIDs(const TypeInfo& typeInfo, const uint8* object, Array<AssetID>& dependencies)
		{
			const Id64 assetIDType = GetTypeID<AssetID>();
			for (uint i = 0; i < typeInfo.fieldCount; ++i)
			{
				const Field& field = typeInfo.fields[i];
				const uint8* data = object + field.dataOffset;
				if (field.typeID == assetIDType && !field.isCArray)
				{
					AddDependency(*reinterpret_cast<const AssetID*>(data), dependencies);
				}
				else if (field.customPropertiesReflector && field.customPropertiesReflector->GetType() == CustomObjectPropertiesReflector::Type::LocalArray)
				{
					const ArrayPropertiesReflector& reflector = static_cast<const ArrayPropertiesReflector&>(*field.customPropertiesReflector);
					if (reflector.GetElementTypeID() == assetIDType)
					{
						void* array = const_cast<uint8*>(data);
						for (uint j = 0; j < reflector.GetSize(data); ++j)
						{
							AddDependency(*static_cast<const AssetID*>(reflector.GetElement(array, j)), dependencies);
						}
					}
				}
				else if (!field.customPropertiesReflector && !field.isCArray)
				{
					if (const TypeInfo* nested = TypeRegistry::Instance().FindType(field.typeID))
					{
						CollectAssetIDs(*nested, data, dependencies);
					}
				}
			}
		}

		void CollectActorDependencies(const World& world, Array<AssetID>& dependencies)
		{
			dependencies.Clear();
			const ComponentRegistry& componentRegistry = ComponentRegistry::Instance();
			for (const ActorInstance& actor : world.actorInstances)
			{
				for (Entity entity : actor.entities)
				{
					world.entities.ForEachComponentType(entity, [&](ComponentTypeID componentTypeID)
					{
						if (const TypeInfo* typeInfo = TypeRegistry::Instance().FindType(componentRegistry.GetReflectionTypeID(componentTypeID)))
						{
							CollectAssetIDs(*typeInfo, static_cast<const uint8*>(world.entities.GetComponentData(entity, componentTypeID)), dependencies);
						}
					});
				}
			}
		}

		// Writes every value that differs from the actor type's default, returning how many.
		uint WriteOverrides(ByteWriter& writer, const World& world, const ActorInstance& actor, const ActorTypeDesc& actorType)
		{
			const Id64 transformType = GetTypeID<ComponentTransform>();
			uint count = 0;
			for (uint i = 0; i < actor.entities.Size() && i < actorType.entities.Size(); ++i)
			{
				const ActorEntityDesc& entityDesc = actorType.entities[i];
				for (const ActorComponentDesc& componentDesc : entityDesc.components)
				{
					// The root's transform is saved with the actor.
					if (i == 0 && componentDesc.typeID == transformType)
					{
						continue;
					}

					ComponentTypeID componentTypeID;
					if (!ComponentRegistry::Instance().FindComponentTypeID(componentDesc.typeID, componentTypeID)
						|| !world.entities.HasComponent(actor.entities[i], componentTypeID))
					{
						continue;
					}

					const uint8* component = static_cast<const uint8*>(world.entities.GetComponentData(actor.entities[i], componentTypeID));
					for (const OverridableField& field : OverridableFields::Get(componentDesc.typeID))
					{
						if (OverridableFields::Equal(field, component, componentDesc.data))
						{
							continue;
						}
						writer.Write(entityDesc.localNameId.GetHash());
						writer.Write(componentDesc.typeID.GetHash());
						writer.Write(field.key.GetHash());
						writer.Write(field.size);
						writer.WriteBytes(component + field.offset, field.size);
						++count;
					}
				}
			}
			return count;
		}

		void WriteActors(ByteWriter& writer, const World& world, const Array<RelativePath>& folders)
		{
			writer.Write(world.actorInstances.Size());
			for (const ActorInstance& actor : world.actorInstances)
			{
				writer.Write(actor.id.GetHash());
				writer.Write(actor.typeID.GetHash());
				writer.WriteString(actor.name.CStr());
				writer.Write(actor.folderPath.Size() > 0 ? FindFolderIndex(folders, actor.folderPath) : c_RootFolder);

				const Entity root = actor.RootEntity();
				const ComponentTypeID transformTypeID = ComponentRegistry::GetComponentTypeID<ComponentTransform>();
				WriteTransform(writer, world.entities.HasComponent(root, transformTypeID)
					? static_cast<const ComponentTransform*>(world.entities.GetComponentData(root, transformTypeID))->local
					: CreateIdentityTransform());

				const uint countOffset = writer.WritePlaceholder();
				const ActorTypeDesc* actorType = ActorRegistry::Instance().FindActorType(actor.typeID);
				writer.Patch(countOffset, actorType ? WriteOverrides(writer, world, actor, *actorType) : 0);
			}
		}
#endif
	}

	bool LevelFile::Load(const char* levelPath, WorldManager& worldManager, Handle worldHandle)
	{
		char absPath[TYR_MAX_PATH_TOTAL_SIZE];
		AssetUtil::CreateFullPath(absPath, levelPath);
		if (!Platform::FileExists(absPath))
		{
			TYR_LOG_ERROR("Level %s doesn't exist.", levelPath);
			return false;
		}

		// Reused between loads, so loading a level doesn't allocate once it's grown to fit.
		static Array<uint8> s_Buffer;
		static Array<RelativePath> s_Folders;
		const size_t size = FileStream::ReadAllFile(absPath, s_Buffer);
		ByteReader reader(s_Buffer.Data(), size);

		if (reader.Read<uint>() != c_Magic)
		{
			TYR_LOG_ERROR("%s isn't a level file.", levelPath);
			return false;
		}
		const uint version = reader.Read<uint>();
		if (version > c_FormatVersion)
		{
			TYR_LOG_ERROR("%s was saved by a newer version of the engine.", levelPath);
			return false;
		}

		s_Folders.Clear();
		World& world = worldManager.GetWorld(worldHandle);
		while (reader.GetRemaining() > 0 && !reader.Failed())
		{
			const uint sectionID = reader.Read<uint>();
			const uint sectionSize = reader.Read<uint>();
			const uint8* sectionData = reader.ReadSpan(sectionSize);
			if (!sectionData)
			{
				break;
			}

			// Unknown sections come from newer versions and are skipped.
			ByteReader section(sectionData, sectionSize);
			switch (sectionID)
			{
			case c_SettingsSection:
				ReadSettings(section, world.settings);
				break;
			case c_FoldersSection:
				ReadFolders(section, s_Folders);
				break;
			case c_ActorsSection:
				ReadActors(section, worldManager, worldHandle, s_Folders);
				break;
			default:
				break;
			}

			if (section.Failed())
			{
				TYR_LOG_ERROR("Part of %s is damaged and couldn't be read.", levelPath);
			}
		}

		for (const RelativePath& folder : s_Folders)
		{
			world.folders.Add(folder);
		}

		if (reader.Failed())
		{
			TYR_LOG_ERROR("%s is damaged, so only part of it was loaded.", levelPath);
		}
		return true;
	}

#if TYR_EDITOR
	AssetID LevelFile::Save(const char* levelPath, const World& world)
	{
		static Array<uint8> s_Buffer;
		static Array<RelativePath> s_Folders;
		static Array<AssetID> s_Dependencies;

		BuildFolders(world, s_Folders);
		CollectActorDependencies(world, s_Dependencies);

		ByteWriter writer(s_Buffer);
		writer.Write(c_Magic);
		writer.Write(c_FormatVersion);

		uint section = BeginSection(writer, c_SettingsSection);
		WriteSettings(writer, world.settings);
		EndSection(writer, section);

		section = BeginSection(writer, c_FoldersSection);
		writer.Write(s_Folders.Size());
		for (const RelativePath& folder : s_Folders)
		{
			writer.WriteString(folder.CStr());
		}
		EndSection(writer, section);

		section = BeginSection(writer, c_DependenciesSection);
		writer.Write(s_Dependencies.Size());
		for (AssetID assetID : s_Dependencies)
		{
			writer.Write(assetID.GetHash());
		}
		EndSection(writer, section);

		section = BeginSection(writer, c_ActorsSection);
		WriteActors(writer, world, s_Folders);
		EndSection(writer, section);

		// Written beside the level first, so a failed save never leaves it half written.
		char absPath[TYR_MAX_PATH_TOTAL_SIZE];
		AssetUtil::CreateFullPath(absPath, levelPath);
		char tempPath[TYR_MAX_PATH_TOTAL_SIZE];
		snprintf(tempPath, sizeof(tempPath), "%s.tmp", absPath);
		PathUtil::CreateDirectoriesInFilePath(absPath);
		FileStream::WriteFile(tempPath, s_Buffer.Data(), s_Buffer.Size());

		if (!Platform::RenameFile(tempPath, absPath))
		{
			TYR_LOG_ERROR("Failed to save level %s.", levelPath);
			return AssetConstants::c_InvalidAssetID;
		}

		return AssetRegistry::Instance().AddOrUpdateAsset(levelPath, s_Dependencies.Data(), s_Dependencies.Size());
	}
#endif
}
