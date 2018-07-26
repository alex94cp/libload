#include "memory.hpp"

#include <algorithm>
#include <stdexcept>
#include <system_error>

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

bool CurrentProcessMemory::allows_direct_addressing() const
{
	return true;
}

void * CurrentProcessMemory::allocate(std::uintptr_t base, std::size_t size)
{
	void * const base_ptr = reinterpret_cast<void *>(base);
	void * const mem = VirtualAlloc(base_ptr, size, MEM_RESERVE, PAGE_NOACCESS);
	if (!mem) throw std::system_error(GetLastError(), std::system_category());
	return mem;
}

void CurrentProcessMemory::release(void * mem, std::size_t size)
{
	if (!VirtualFree(mem, 0, MEM_RELEASE))
		throw std::system_error(GetLastError(), std::system_category());
}

void CurrentProcessMemory::commit(void * mem, std::size_t size)
{
	if (!VirtualAlloc(mem, size, MEM_COMMIT, PAGE_READWRITE))
		throw std::system_error(GetLastError(), std::system_category());
}

void CurrentProcessMemory::decommit(void * mem, std::size_t size)
{
	if (!VirtualFree(mem, size, MEM_DECOMMIT))
		throw std::system_error(GetLastError(), std::system_category());
}

void CurrentProcessMemory::set_access(void * mem, std::size_t size, int access)
{
	DWORD old_protect;
	const DWORD mem_flags = memory_access_to_win32(access);
	if (!VirtualProtect(mem, size, mem_flags, &old_protect))
		throw std::system_error(GetLastError(), std::system_category());
}

std::size_t CurrentProcessMemory::copy_from(const void * mem, std::size_t size, void * into_buffer)
{
	std::copy_n(static_cast<const char *>(mem), size, static_cast<char *>(into_buffer));
	return size;
}

std::size_t CurrentProcessMemory::copy_into(const void * data, std::size_t size, void * into_mem)
{
	std::copy_n(static_cast<const char *>(data), size, static_cast<char *>(into_mem));
	return size;
}

LocalProcessMemory::LocalProcessMemory(HANDLE handle)
	: _handle { handle } {}

bool LocalProcessMemory::allows_direct_addressing() const
{
	return false;
}

void * LocalProcessMemory::allocate(std::uintptr_t base, std::size_t size)
{
	void * const base_ptr = reinterpret_cast<void *>(base);
	void * const mem = VirtualAllocEx(_handle, base_ptr, size, MEM_RESERVE, PAGE_NOACCESS);
	if (mem == nullptr) throw std::system_error(GetLastError(), std::system_category());
	return mem;
}

void LocalProcessMemory::release(void * mem, std::size_t size)
{
	if (!VirtualFreeEx(_handle, mem, 0, MEM_RELEASE))
		throw std::system_error(GetLastError(), std::system_category());
}

void LocalProcessMemory::commit(void * mem, std::size_t size)
{
	if (!VirtualAllocEx(_handle, mem, size, MEM_COMMIT, PAGE_READWRITE))
		throw std::system_error(GetLastError(), std::system_category());
}

void LocalProcessMemory::decommit(void * mem, std::size_t size)
{
	if (!VirtualFreeEx(_handle, mem, size, MEM_DECOMMIT))
		throw std::system_error(GetLastError(), std::system_category());
}

void LocalProcessMemory::set_access(void * mem, std::size_t size, int access)
{
	DWORD old_protect;
	const DWORD mem_flags = memory_access_to_win32(access);
	if (!VirtualProtectEx(_handle, mem, size, mem_flags, &old_protect))
		throw std::system_error(GetLastError(), std::system_category());
}

std::size_t LocalProcessMemory::copy_from(const void * mem, std::size_t size, void * into_buffer)
{
	SIZE_T bytes_read;
	if (!ReadProcessMemory(_handle, mem, into_buffer, size, &bytes_read))
		throw std::system_error(GetLastError(), std::system_category());
	return bytes_read;
}

std::size_t LocalProcessMemory::copy_into(const void * from_buffer, std::size_t size, void * into_mem)
{
	SIZE_T bytes_written;
	if (!WriteProcessMemory(_handle, into_mem, from_buffer, size, &bytes_written))
		throw std::system_error(GetLastError(), std::system_category());
	return bytes_written;
}

}