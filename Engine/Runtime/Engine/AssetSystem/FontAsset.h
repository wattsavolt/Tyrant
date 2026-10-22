#pragma once

#include "EngineMacros.h"
#include "Core.h"
#include "Containers/Array.h"
#include "IO/BufferedFileStream.h"

namespace tyr
{
	// A TrueType or OpenType font file, kept as it is. The UI bakes its letters into a texture at
	// whatever sizes it needs. Only lives on the CPU.
	struct FontFile
	{
		// Written first and bumped whenever the layout changes. The high half is a tag.
		static constexpr uint c_FileVersion = 0x464E0001;

		Array<uint8> data;

		void Reset() { data.Clear(); }
	};

	template<> void Serialize(BufferedFileStream& stream, const FontFile& font);
	template<> void Deserialize(BufferedFileStream& stream, FontFile& font);
}
