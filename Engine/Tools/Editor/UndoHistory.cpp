#include "UndoHistory.h"
#include "World/WorldManager.h"
#include "Level/LevelFile.h"
#include "ECS/Components.h"
#include <algorithm>
#include <cstring>

namespace tyr
{
	UndoHistory::UndoHistory(WorldManager& worldManager, Handle levelWorld)
		: m_WorldManager(worldManager)
		, m_LevelWorld(levelWorld)
		, m_History(Array<uint8>::UninitializedTag{}, c_HistoryCapacity)
	{
		for (Snapshot& snapshot : m_Snapshots)
		{
			snapshot.data.Resize(Array<uint8>::UninitializedTag{}, c_SnapshotCapacity);
			snapshot.entries.Resize(Array<SnapshotEntry>::UninitializedTag{}, c_MaxSnapshotActors);
		}
		Reset();
	}

	void UndoHistory::Reset()
	{
		m_FirstStep = 0;
		m_StepCount = 0;
		m_CurrentStep = 0;
		m_Enabled = TakeSnapshot(m_Snapshots[m_CommittedSnapshot]);
		if (!m_Enabled)
		{
			TYR_LOG_WARNING("The level is too big for undo, so it's off until the level gets smaller.");
		}
		m_CommittedChangeCount = m_WorldManager.GetWorld(m_LevelWorld).changeCount;
	}

	void UndoHistory::Commit()
	{
		const World& world = m_WorldManager.GetWorld(m_LevelWorld);
		if (world.changeCount == m_CommittedChangeCount)
		{
			return;
		}
		m_CommittedChangeCount = world.changeCount;

		// Back on once the level fits again, starting from how it is now.
		if (!m_Enabled)
		{
			m_Enabled = TakeSnapshot(m_Snapshots[m_CommittedSnapshot]);
			return;
		}

		const Snapshot& before = m_Snapshots[m_CommittedSnapshot];
		Snapshot& after = m_Snapshots[1 - m_CommittedSnapshot];
		if (!TakeSnapshot(after))
		{
			TYR_LOG_WARNING("The level is too big for undo, so it's off until the level gets smaller.");
			m_Enabled = false;
			m_FirstStep = 0;
			m_StepCount = 0;
			m_CurrentStep = 0;
			return;
		}

		uint stepSize = 0;
		Compare(before, after, [&stepSize](uint64 /*id*/, const uint8* /*beforeState*/, uint beforeSize, const uint8* /*afterState*/, uint afterSize)
		{
			stepSize += static_cast<uint>(sizeof(ChangeHeader)) + beforeSize + afterSize;
		});

		// An edit that put everything back the way it was isn't a step.
		if (stepSize > 0 && stepSize <= c_HistoryCapacity)
		{
			// A new edit replaces whatever could have been redone.
			m_StepCount = m_CurrentStep;
			const uint offset = AllocateStep(stepSize);
			uint8* out = m_History.Data() + offset;
			Compare(before, after, [&out](uint64 id, const uint8* beforeState, uint beforeSize, const uint8* afterState, uint afterSize)
			{
				const ChangeHeader header = { id, beforeSize, afterSize };
				memcpy(out, &header, sizeof(header));
				out += sizeof(header);
				memcpy(out, beforeState, beforeSize);
				out += beforeSize;
				memcpy(out, afterState, afterSize);
				out += afterSize;
			});

			GetStep(m_StepCount) = { offset, stepSize };
			++m_StepCount;
			m_CurrentStep = m_StepCount;
		}
		else if (stepSize > c_HistoryCapacity)
		{
			TYR_LOG_WARNING("That change was too big to undo, so the undo history was cleared.");
			m_FirstStep = 0;
			m_StepCount = 0;
			m_CurrentStep = 0;
		}
		m_CommittedSnapshot = 1 - m_CommittedSnapshot;
	}

	bool UndoHistory::Undo()
	{
		Commit();
		if (!CanUndo())
		{
			return false;
		}
		--m_CurrentStep;
		ApplyStep(GetStep(m_CurrentStep), true);
		return true;
	}

	bool UndoHistory::Redo()
	{
		Commit();
		if (!CanRedo())
		{
			return false;
		}
		ApplyStep(GetStep(m_CurrentStep), false);
		++m_CurrentStep;
		return true;
	}

	bool UndoHistory::TakeSnapshot(Snapshot& snapshot)
	{
		const World& world = m_WorldManager.GetWorld(m_LevelWorld);
		snapshot.size = 0;
		snapshot.entryCount = 0;
		if (world.actorInstances.Size() >= c_MaxSnapshotActors)
		{
			return false;
		}

		const uint levelSize = LevelFile::WriteLevelState(world, snapshot.data.Data(), c_SnapshotCapacity);
		if (levelSize == 0)
		{
			return false;
		}
		snapshot.entries[snapshot.entryCount++] = { 0, 0, levelSize };
		snapshot.size = levelSize;

		for (const ActorInstance& actor : world.actorInstances)
		{
			const uint size = LevelFile::WriteActorState(world, actor, snapshot.data.Data() + snapshot.size, c_SnapshotCapacity - snapshot.size);
			if (size == 0)
			{
				return false;
			}
			snapshot.entries[snapshot.entryCount++] = { actor.id.GetHash(), snapshot.size, size };
			snapshot.size += size;
		}

		// Sorted by ID, so two snapshots can be compared in one pass.
		std::sort(snapshot.entries.Data(), snapshot.entries.Data() + snapshot.entryCount, [](const SnapshotEntry& a, const SnapshotEntry& b)
		{
			return a.id < b.id;
		});
		return true;
	}

	template<typename OnChange>
	void UndoHistory::Compare(const Snapshot& before, const Snapshot& after, OnChange change) const
	{
		uint i = 0;
		uint j = 0;
		while (i < before.entryCount || j < after.entryCount)
		{
			const SnapshotEntry* a = i < before.entryCount ? &before.entries[i] : nullptr;
			const SnapshotEntry* b = j < after.entryCount ? &after.entries[j] : nullptr;
			if (a && (!b || a->id < b->id))
			{
				change(a->id, before.data.Data() + a->offset, a->size, nullptr, 0);
				++i;
			}
			else if (!a || b->id < a->id)
			{
				change(b->id, nullptr, 0, after.data.Data() + b->offset, b->size);
				++j;
			}
			else
			{
				const uint8* beforeState = before.data.Data() + a->offset;
				const uint8* afterState = after.data.Data() + b->offset;
				if (a->size != b->size || memcmp(beforeState, afterState, a->size) != 0)
				{
					change(a->id, beforeState, a->size, afterState, b->size);
				}
				++i;
				++j;
			}
		}
	}

	void UndoHistory::ApplyStep(const StepRange& step, bool undo)
	{
		// Actors are removed first, so the lights they had make room for any being put back.
		const uint8* stepData = m_History.Data() + step.offset;
		for (uint pass = 0; pass < 2; ++pass)
		{
			uint offset = 0;
			while (offset < step.size)
			{
				ChangeHeader header;
				memcpy(&header, stepData + offset, sizeof(header));
				const uint8* beforeState = stepData + offset + sizeof(header);
				const uint8* afterState = beforeState + header.beforeSize;
				offset += static_cast<uint>(sizeof(header)) + header.beforeSize + header.afterSize;

				const uint8* state = undo ? beforeState : afterState;
				const uint size = undo ? header.beforeSize : header.afterSize;
				if ((size == 0) == (pass == 0))
				{
					ApplyState(header.id, state, size, undo ? afterState : beforeState, undo ? header.afterSize : header.beforeSize);
				}
			}
		}

		// The level now matches the step, so it's the new starting point for the next one.
		m_Enabled = TakeSnapshot(m_Snapshots[m_CommittedSnapshot]);
		m_CommittedChangeCount = m_WorldManager.GetWorld(m_LevelWorld).changeCount;
	}

	void UndoHistory::ApplyState(uint64 id, const uint8* state, uint size, const uint8* currentState, uint currentSize)
	{
		World& world = m_WorldManager.GetWorld(m_LevelWorld);
		if (id == 0)
		{
			LevelFile::ReadLevelState(state, size, world);
			return;
		}

		ActorInstance* actor = nullptr;
		for (ActorInstance& instance : world.actorInstances)
		{
			if (instance.id.GetHash() == id)
			{
				actor = &instance;
				break;
			}
		}

		if (size == 0)
		{
			if (actor)
			{
				m_WorldManager.RemoveActorInstance(m_LevelWorld, actor->RootEntity());
			}
			return;
		}

		if (actor)
		{
			// Only moved, renamed or put in another folder, so it's changed in place rather than
			// made again and its mesh doesn't have to reload.
			LevelFile::ActorStateHeader target;
			LevelFile::ActorStateHeader current;
			const Entity root = actor->RootEntity();
			if (currentSize > 0
				&& LevelFile::ReadActorStateHeader(state, size, target)
				&& LevelFile::ReadActorStateHeader(currentState, currentSize, current)
				&& target.typeID == current.typeID
				&& size - target.overridesOffset == currentSize - current.overridesOffset
				&& memcmp(state + target.overridesOffset, currentState + current.overridesOffset, size - target.overridesOffset) == 0
				&& world.entities.HasComponent<ComponentTransform>(root))
			{
				actor->name = target.name;
				actor->folderPath = target.folder;
				m_WorldManager.SetActorTransform(m_LevelWorld, root, target.transform);
				return;
			}
			m_WorldManager.RemoveActorInstance(m_LevelWorld, root);
		}
		LevelFile::AddActorFromState(state, size, m_WorldManager, m_LevelWorld);
	}

	uint UndoHistory::AllocateStep(uint size)
	{
		// Straight after the newest step, or back at the start if it won't fit before the end.
		uint offset = 0;
		if (m_StepCount > 0)
		{
			const StepRange& newest = GetStep(m_StepCount - 1);
			offset = newest.offset + newest.size;
		}
		if (size > c_HistoryCapacity - offset)
		{
			offset = 0;
		}

		// The steps in the way are always the oldest ones, as steps are written in order.
		while (m_StepCount > 0)
		{
			const StepRange& oldest = GetStep(0);
			const bool overlaps = oldest.offset < offset + size && offset < oldest.offset + oldest.size;
			if (!overlaps && m_StepCount < c_MaxSteps)
			{
				break;
			}
			m_FirstStep = (m_FirstStep + 1) % c_MaxSteps;
			--m_StepCount;
			--m_CurrentStep;
		}
		return offset;
	}
}
