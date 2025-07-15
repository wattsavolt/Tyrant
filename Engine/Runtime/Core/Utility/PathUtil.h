#pragma once

#include "Base/Base.h"
#include "String/StringTypes.h"

namespace tyr
{
	class TYR_CORE_EXPORT PathUtil 
	{
    private:
        static void CombineImpl(char[], size_t&)
        {
            // base case: no paths left, do nothing
        }

        template<typename First, typename... Rest>
        static void CombineImpl(char combinedPath[], size_t& pos, First&& first, Rest&&... rest)
        {
            const char* segment = first;
            const size_t len = std::strlen(segment);

            if (len > 0)
            {
                // Copy current segment
                std::memcpy(combinedPath + pos, segment, len);
                pos += len;

                // Add '/' if there are more paths and no trailing slash
                if (sizeof...(rest) > 0)
                {
                    const char lastChar = segment[len - 1];
                    if (lastChar != '/' && lastChar != '\\')
                    {
                        combinedPath[pos++] = '/';
                    }
                }
            }

            CombineImpl(combinedPath, pos, std::forward<Rest>(rest)...);
        }

	public:
		// fileName must be preallocated
		static void GetFileNameWithoutExtension(char fileName[], const char* filePath);

        static String GetFileNameWithoutExtension(const String& input);

        // Combines multiple paths
        // Handy for tools
        template<typename... Paths>
        static void Combine(char combinedPath[], Paths&&... paths)
        {
            size_t pos = 0;
            CombineImpl(combinedPath, pos, std::forward<Paths>(paths)...);
            combinedPath[pos] = '\0';

            // Normalize slashes
            for (size_t i = 0; i < pos; ++i)
            {
                if (combinedPath[i] == '\\')
                {
                    combinedPath[i] = '/';
                }
            }
        }

        // Creates all the directories in the file path if they do not exists
        static void CreateDirectoriesInFilePath(const char* filePath);
	};
}