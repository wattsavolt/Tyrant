#pragma once

#include "Win32Platform.h"
#include <windows.h>
#include "String/StringTypes.h"
#include "Identifiers/Guid.h"
#include "Utility/PathUtil.h"
#include <intrin.h>
#include <rpc.h>

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

	size_t Platform::GetSizeOfFile(FileHandle handle)
	{
		TYR_ASSERT(handle);
		DWORD fileSizeHigh;
		const DWORD fileSizeLow = GetFileSize(handle, &fileSizeHigh);

		if (fileSizeLow == INVALID_FILE_SIZE)
		{
			TYR_ASSERT(false);
			return 0;
		}
		
		const size_t fileSize = (static_cast<size_t>(fileSizeHigh) << 32) | fileSizeLow;
		return fileSize;
	}

	void Platform::SetFilePosition(FileHandle handle, size_t position)
	{
		const DWORD newPos = SetFilePointer(handle, position, NULL, FILE_BEGIN);
		TYR_ASSERT(newPos != INVALID_SET_FILE_POINTER && GetLastError() == NO_ERROR);
	}

	void Platform::SetFilePositionToEnd(FileHandle handle)
	{
		const DWORD newPos = SetFilePointer(handle, 0, NULL, FILE_END);
		TYR_ASSERT(newPos != INVALID_SET_FILE_POINTER && GetLastError() == NO_ERROR);
	}

	size_t Platform::GetFilePosition(FileHandle handle)
	{
		const DWORD pos = SetFilePointer(handle, 0, NULL, FILE_CURRENT);
		TYR_ASSERT(pos != INVALID_SET_FILE_POINTER && GetLastError() == NO_ERROR); 
		return pos;
	}

	bool Platform::IsEOF(FileHandle handle)
	{
		const DWORD end = SetFilePointer(handle, 0, NULL, FILE_END);
		TYR_ASSERT(end != INVALID_SET_FILE_POINTER && GetLastError() == NO_ERROR);
		return end == static_cast<DWORD>(GetFilePosition(handle));
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
}