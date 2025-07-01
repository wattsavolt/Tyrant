#pragma once

#include "PathUtil.h"

namespace tyr
{
    void PathUtil::GetFileNameWithoutExtension(char fileName[], const char* filePath)
    {
        // Step 1: Find the last path separator ('/' or '\\')
        const char* lastSlash = strrchr(filePath, '/');
#ifdef _WIN32
        const char* lastBackslash = strrchr(filePath, '\\');
        if (!lastSlash || (lastBackslash && lastBackslash > lastSlash))
            lastSlash = lastBackslash;
#endif

        const char* filenameStart = lastSlash ? lastSlash + 1 : filePath;

        // Find the last dot after the filename start
        const char* lastDot = strrchr(filenameStart, '.');

        // Copy filename without extension
        const size_t length = lastDot ? (size_t)(lastDot - filenameStart) : strlen(filenameStart);
        std::memcpy(fileName, filenameStart, length);
        fileName[length] = '\0'; // Null-terminate
    }

    String PathUtil::GetFileNameWithoutExtension(const String& input)
    {
        TYR_ASSERT(input != ".");
        size_t lastindex = input.find_last_of(".");
        return input.substr(0, lastindex);
    }
}