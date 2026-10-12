#pragma once

#include "Core.h"

namespace tyr
{
	class WorldManager;
	struct World;

	// The level editor's undo and redo. Each step holds the before and after states of the actors
	// it changed, found by comparing the level with a snapshot of how it was after the last step.
	// All of its memory is allocated up front, and the oldest steps are dropped to make room.
	class UndoHistory final
	{
	public:
		UndoHistory(WorldManager& worldManager, Handle levelWorld);

		// Forgets every step and starts again from the level as it is now, such as after opening one.
		void Reset();

		// Records the edits made since the last step as a new one, if there were any. Called once
		// nothing is being dragged or typed, so a whole drag becomes one step.
		void Commit();

		bool CanUndo() const { return m_CurrentStep > 0; }
		bool CanRedo() const { return m_CurrentStep < m_StepCount; }

		// Each records any edits not yet in a step first. Return false when there's nothing to do.
		bool Undo();
		bool Redo();

	private:
		// An actor's state in a snapshot. The level's own state is kept with an ID of 0.
		struct SnapshotEntry
		{
			uint64 id;
			uint offset;
			uint size;
		};

		struct Snapshot
		{
			// Both allocated to their full capacity up front.
			Array<uint8> data;
			Array<SnapshotEntry> entries;
			uint size = 0;
			uint entryCount = 0;
		};

		// Where a step sits in the history buffer.
		struct StepRange
		{
			uint offset;
			uint size;
		};

		// What each change in a step starts with, followed by its before and after states. A
		// state with a size of 0 means the actor didn't exist.
		struct ChangeHeader
		{
			uint64 id;
			uint beforeSize;
			uint afterSize;
		};

		// Fills the snapshot with the level as it is now. Fails if it doesn't fit.
		bool TakeSnapshot(Snapshot& snapshot);
		// Calls change(id, before, beforeSize, after, afterSize) for each actor that differs.
		template<typename OnChange>
		void Compare(const Snapshot& before, const Snapshot& after, OnChange change) const;
		// Puts the actor or level back to the state, removing the actor when the size is 0.
		void ApplyState(uint64 id, const uint8* state, uint size, const uint8* currentState, uint currentSize);
		// Applies the step's before states when undoing, or its after states when redoing.
		void ApplyStep(const StepRange& step, bool undo);
		// Makes room at the end of the history for a new step, dropping the oldest as needed.
		uint AllocateStep(uint size);
		StepRange& GetStep(uint index) { return m_Steps[(m_FirstStep + index) % c_MaxSteps]; }

		static constexpr uint c_HistoryCapacity = 32 * 1024 * 1024;
		static constexpr uint c_SnapshotCapacity = 8 * 1024 * 1024;
		static constexpr uint c_MaxSnapshotActors = 32768;
		static constexpr uint c_MaxSteps = 512;

		WorldManager& m_WorldManager;
		Handle m_LevelWorld;
		// The level as of the current step, and the one taken to compare with it.
		Snapshot m_Snapshots[2];
		uint m_CommittedSnapshot = 0;
		// The level's change count when the committed snapshot was taken.
		uint m_CommittedChangeCount = 0;
		// Off when the level is too big to snapshot, until the next Reset.
		bool m_Enabled = true;

		Array<uint8> m_History;
		StepRange m_Steps[c_MaxSteps];
		uint m_FirstStep = 0;
		uint m_StepCount = 0;
		// How many of the steps are applied. Those after it can be redone.
		uint m_CurrentStep = 0;
	};
}
