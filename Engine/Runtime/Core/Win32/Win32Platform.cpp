#pragma once

#include "Win32Platform.h"
#include <windows.h>
#include "String/StringTypes.h"
#include "Identifiers/Guid.h"
#include "Utility/PathUtil.h"
#include <intrin.h>
#include <rpc.h>
#include <shlobj.h>

namespace tyr
{
	const char* Platform::c_DynamicLibExtension = ".dll";

	const String Platform::c_BinaryDirectory = GetBinaryDirectoryPath();

	void Platform::Exit(bool cleanup)
	{
		if (cleanup)
		{
			PostQuitMessage(0);
		}
		else
		{
			TerminateProcess(GetCurrentProcess(), 0);
		}
	}

	Handle Platform::OpenLibrary(const char* filename, bool addFileExtension)
	{
		String filePath = String(filename);
		if (addFileExtension)
		{
			filePath += Platform::c_DynamicLibExtension;
		}
		return LoadLibrary(filePath.c_str());
	}

	bool Platform::CloseLibrary(Handle library)
	{
		return FreeLibrary(static_cast<HMODULE>(library));
	}

	void* Platform::GetProcessAddress(const Handle library, const char* function)
	{
		return GetProcAddress((HMODULE)library, function);
	}

	bool Platform::IsDebuggerAttached()
	{
		return IsDebuggerPresent();
	}

	int Platform::GetScreenWidth()
	{
		return GetSystemMetrics(SM_CXSCREEN);
	}

	int Platform::GetScreenHeight()
	{
		return GetSystemMetrics(SM_CYSCREEN);
	}

	uint64 Platform::GetCpuCycles()
	{
		int a[4];
		int b = 0;
		__cpuid(a, b);
		return __rdtsc();
	}

	FileHandle Platform::OpenOrCreateFile(const char* filename, FileAccess access, FileCreationMode creationMode)
	{
		DWORD desiredAccess;
		switch (access)
		{
		case FileAccess::Read:
			desiredAccess = GENERIC_READ;
			break;
		case FileAccess::Write:
			desiredAccess = GENERIC_WRITE;
			break;
		case FileAccess::Execute:
			desiredAccess = GENERIC_EXECUTE;
			break;
		case FileAccess::All:
			desiredAccess = GENERIC_ALL;
			break;
		default:
			TYR_ASSERT(false);
			return nullptr;
		}

		DWORD creationDisposition;
		switch (creationMode)
		{
		case FileCreationMode::CreateAlways:
			creationDisposition = CREATE_ALWAYS;
			break;
		case FileCreationMode::CreateNew:
			creationDisposition = CREATE_NEW;
			break;
		case FileCreationMode::OpenAlways:
			creationDisposition = OPEN_ALWAYS;
			break;
		case FileCreationMode::OpenExisting:
			creationDisposition = OPEN_EXISTING;
			break;
		case FileCreationMode::TruncateExisting:
			creationDisposition = TRUNCATE_EXISTING;
			break;
		default:
			TYR_ASSERT(false);
			return nullptr;
		}
		
		HANDLE handle = CreateFile(filename, desiredAccess, 0, nullptr, creationDisposition, FILE_ATTRIBUTE_NORMAL, nullptr);

		TYR_ASSERT(handle && handle != INVALID_HANDLE_VALUE);
		
		return handle;
	}

	static inline LONGLONG Seek(HANDLE h, LONGLONG offset, DWORD moveMethod)
	{
		LARGE_INTEGER off, pos;
		off.QuadPart = offset;
		const BOOL ok = SetFilePointerEx(h, off, &pos, moveMethod);
		TYR_ASSERT(ok);
		return pos.QuadPart;
	}

	size_t Platform::GetSizeOfFile(FileHandle handle)
	{
		TYR_ASSERT(handle);

		DWORD fileSizeHigh;
		const DWORD fileSizeLow = GetFileSize(handle, &fileSizeHigh);

		if (fileSizeLow == INVALID_FILE_SIZE)
		{
			DWORD err = GetLastError();
			TYR_ASSERT(err == NO_ERROR); // Only fail if there really is an error
			if (err != NO_ERROR)
				return 0;
		}

		// Use 64-bit to combine high and low
		const uint64 fullSize = (static_cast<uint64_t>(fileSizeHigh) << 32) | fileSizeLow;
		return static_cast<size_t>(fullSize); // safe if size_t >= 64-bit
	}

	void Platform::SetFilePosition(FileHandle handle, size_t position)
	{
		Seek(handle, position, FILE_BEGIN);
	}

	void Platform::SetFilePositionToEnd(FileHandle handle)
	{
		Seek(handle, 0, FILE_END);
	}

	size_t Platform::GetFilePosition(FileHandle handle)
	{
		return static_cast<size_t>(Seek(handle, 0, FILE_CURRENT));
	}

	bool Platform::IsEOF(FileHandle handle)
	{
		LONGLONG end = Seek(handle, 0, FILE_END);
		LONGLONG cur = Seek(handle, 0, FILE_CURRENT);
		return end == cur;
	}


	void Platform::ReadFromFile(FileHandle handle, uint8* buffer, size_t numberOfBytesToRead, size_t& bytesRead) 
	{
		TYR_ASSERT(handle);
		DWORD bytesReadTemp;
		const bool result = ReadFile(handle, static_cast<LPVOID>(buffer), numberOfBytesToRead, &bytesReadTemp, nullptr);
		if (result)
		{
			bytesRead = static_cast<size_t>(bytesReadTemp);
		}
		else
		{
			TYR_ASSERT(false);
			bytesRead = 0;
		}
	}

	void Platform::WriteToFile(FileHandle handle, const uint8* buffer, size_t bufferSize, size_t& bytesWritten)
	{
		TYR_ASSERT(handle);
		DWORD written;
		WriteFile(handle, buffer, static_cast<DWORD>(bufferSize), &written, nullptr);
		TYR_ASSERT(written == bufferSize);
		bytesWritten = written;
	}

	void Platform::CloseFile(FileHandle handle)
	{
		TYR_ASSERT(handle);
		TYR_ASSERT(CloseHandle(handle));
	}

	void Platform::ShowAlertMessage(const char* msg)
	{
		MessageBox(NULL, msg, "Alert!", MB_OK | MB_ICONINFORMATION);
	}

	void Platform::CreateGuid(Guid& guid)
	{
		const RPC_STATUS status = UuidCreate(reinterpret_cast<UUID*>(&guid));
		TYR_ASSERT(status == RPC_S_OK);
	}

	void Platform::GetBinaryDirectoryPath(char* dirPath)
	{
		char executablePath[TYR_MAX_PATH];
		GetModuleFileName(nullptr, executablePath, TYR_MAX_PATH);
		PathUtil::GetDirectoryPathFromFilePath(executablePath, dirPath);
	}

	String Platform::GetBinaryDirectoryPath()
	{
		char buffer[TYR_MAX_PATH];
		GetModuleFileName(nullptr, buffer, TYR_MAX_PATH);

		String executablePath = buffer;
		String executableDirectory = executablePath.substr(0, executablePath.find_last_of("\\/"));
		return executableDirectory;
	}

	void Platform::GetUserDirectoryPath(char* dirPath)
	{
		PWSTR wPath = nullptr;

		if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Profile, 0, NULL, &wPath)))
		{
			// Convert wide path to UTF-8 
			int len = WideCharToMultiByte(
				CP_UTF8,            
				0,
				wPath,
				-1,
				dirPath,
				MAX_PATH,
				NULL,
				NULL
			);

			if (len == 0)
				dirPath[0] = '\0';

			CoTaskMemFree(wPath);
			return;
		}

		dirPath[0] = '\0';
	}
}