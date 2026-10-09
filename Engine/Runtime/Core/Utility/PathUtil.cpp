#pragma once

#include "PathUtil.h"
#include "Platform/Platform.h"

namespace tyr
{
    void PathUtil::GetFileNameWithoutExtension(const char* filePath, char* fileName)
    {
        const char* lastSlash = nullptr;
        const char* str = filePath;
        while (*str != '\0')
        {
            if (*str == '/' || *str == '\\')
            {
                lastSlash = str; 
            }
            ++str;
        }

        const char* fileNameStart = lastSlash ? lastSlash + 1 : filePath;

        // Find the last dot after the filename start
        const char* lastDot = strrchr(fileNameStart, '.');

        // Copy filename without extension
        const size_t length = lastDot ? (size_t)(lastDot - fileNameStart) : strlen(fileNameStart);
        memcpy(fileName, fileNameStart, length);
        fileName[length] = '\0'; // Null-terminate
    }

    String PathUtil::GetFileNameWithoutExtension(const String& input)
    {
        TYR_ASSERT(input != ".");
        size_t lastindex = input.find_last_of(".");
        return input.substr(0, lastindex);
    }

    namespace
    {
        // Each directory is made in turn by cutting the path short at its separator.
        void CreateParentDirectories(char* path)
        {
            for (char* c = path; *c != '\0'; ++c)
            {
                if ((*c != '/' && *c != '\\') || c == path || *(c - 1) == ':')
                {
                    continue;
                }
                const char separator = *c;
                *c = '\0';
                Platform::MakeDirectory(path);
                *c = separator;
            }
        }
    }

    void PathUtil::CreateDirectoriesInFilePath(const char* filePath)
    {
        char path[TYR_MAX_PATH_TOTAL_SIZE];
        strcpy_s(path, filePath);
        CreateParentDirectories(path);
    }

    bool PathUtil::CreateDirectories(const char* dirPath)
    {
        char path[TYR_MAX_PATH_TOTAL_SIZE];
        strcpy_s(path, dirPath);
        CreateParentDirectories(path);
        return Platform::MakeDirectory(path);
    }

    void PathUtil::GetDirectoryPathFromFilePath(const char* filePath, char* dirPath)
    {
        const char* lastSlash = nullptr;
        const char* str = filePath;

        while (*str != '\0')
        {
            if (*str == '/' || *str == '\\')
            {
                lastSlash = str;
            }
            ++str;
        }
        
        if (lastSlash)
        {
            // Copy everything up to (but not including) the last slash
            size_t dirLength = static_cast<size_t>(lastSlash - filePath);
            memcpy(dirPath, filePath, dirLength);
            dirPath[dirLength] = '\0';
        }
        else
        {
            // No slashes -> no directory, return empty string
            dirPath[0] = '\0';
        }
    }
}