#pragma once

#include "CoreMacros.h"
#include "Base/Base.h"
#include "String/StringTypes.h"

namespace tyr
{
	enum class FileAccess
	{
		Read,
		Write,
		Execute,
		All
	};

	enum class FileCreationMode
	{
		CreateAlways,
		CreateNew,
		OpenAlways,
		OpenExisting,
		TruncateExisting
	};

	struct Guid;
	class TYR_CORE_API Platform
	{
	public:
		/// Terminates the application with the option to perform cleanup. 
		[[noreturn]] static void Exit(bool cleanup = true);

		/// Explicitly opens a dynamic library. 
		static void* OpenLibrary(const char* filename, bool addFileExtension = true);

		/// Closes a dynamic library. 
		static bool CloseLibrary(void* library);

		/// Returns the address of a function in a dynamic library. 
		static void* GetProcessAddress(const void* library, const char* function);

		/// Returns true if the application has an attached debugger. 
		static bool IsDebuggerAttached();

		static int GetScreenWidth();

		static int GetScreenHeight();

		static void GetMaxWindowResolution(int& width, int& height, float aspectRatio = 16.0f / 9.0f);

		static uint64 GetCpuCycles();

		static FileHandle OpenOrCreateFile(const char* filename, FileAccess access, FileCreationMode creationMode);

		static size_t GetSizeOfFile(FileHandle handle);

		static void SetFilePosition(FileHandle handle, size_t position);

		static void SetFilePositionToEnd(FileHandle handle);

		static size_t GetFilePosition(FileHandle handle);

		static bool IsEOF(FileHandle handle);

		static void ReadFromFile(FileHandle handle, uint8* buffer, size_t numberOfBytesToRead, size_t& bytesRead);

		static void WriteToFile(FileHandle handle, const uint8* buffer, size_t bufferSize, size_t& bytesWritten);

		static void CloseFile(FileHandle handle);

		static void ShowAlertMessage(const char* msg);

		static void CreateGuid(Guid& guid);

		// @note: dir must be preallocated to a large enough size
		static void GetBinaryDirectoryPath(char* dirPath);

		static String GetBinaryDirectoryPath();

		// Should return path to home folder on Linux
		static void GetUserDirectoryPath(char* dirPath);

		static const char* c_DynamicLibExtension;

		static const String c_BinaryDirectory;
	};
}



