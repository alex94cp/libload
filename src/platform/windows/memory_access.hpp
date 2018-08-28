#ifndef LOAD_SRC_PLATFORM_WINDOWS_MEMORYACCESS_HPP_
#define LOAD_SRC_PLATFORM_WINDOWS_MEMORYACCESS_HPP_

#include <load/memory/memory_manager.hpp>

#include <windows.h>

namespace load::detail {

constexpr DWORD memory_access_to_win32(int mem_access)
{
	if (mem_access & MemoryManager::ReadAccess) {
		if (mem_access & MemoryManager::WriteAccess) {
			if (mem_access & MemoryManager::ExecuteAccess)
				return PAGE_EXECUTE_READWRITE;
			else
				return PAGE_READWRITE;
		} else {
			if (mem_access & MemoryManager::ExecuteAccess)
				return PAGE_EXECUTE_READ;
			else
				return PAGE_READONLY;
		}
	} else {
		return PAGE_NOACCESS;
	}
}

}

#endif