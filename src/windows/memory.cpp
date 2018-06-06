#include <load/memory.hpp>

#include <windows.h>

#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <system_error>

namespace load {

class LocalMemoryManager final : public MemoryManager
{
public:
	virtual bool allows_direct_addressing() const override;

	virtual void * allocate(std::uintptr_t base, std::size_t size) override;
	virtual void set_access(void * mem, std::size_t size, int access) override;
	virtual void release(void * mem, std::size_t size) override;

	virtual std::size_t copy_from(const void * mem, std::size_t size, void * into_buffer) override;
	virtual std::size_t copy_into(const void * data, std::size_t size, void * into_mem) override;

	virtual void commit(void * mem, std::size_t size) override;
	virtual void decommit(void * mem, std::size_t size) override;

	virtual void register_exception_handlers(const void * base, void * entries, std::size_t count) override;
	virtual void deregister_exception_handlers(void * entries, std::size_t count) override;

	virtual std::future<int> run_async(const void * mem, void * param) override;
};

MemoryManager & local_memory_manager = LocalMemoryManager {};

namespace {

constexpr DWORD memory_access_to_win32(int mem_access)
{
	if (mem_access & MemAccessRead) {
		if (mem_access & MemAccessWrite) {
			if (mem_access & MemAccessExecute)
				return PAGE_EXECUTE_READWRITE;
			else
				return PAGE_READWRITE;
		} else {
			if (mem_access & MemAccessExecute)
				return PAGE_EXECUTE_READ;
			else
				return PAGE_READONLY;
		}
	} else {
		return PAGE_NOACCESS;
	}
}

}

bool LocalMemoryManager::allows_direct_addressing() const
{
	return true;
}

void * LocalMemoryManager::allocate(std::uintptr_t base, std::size_t size)
{
	void * const base_ptr = reinterpret_cast<void *>(base);
	void * const mem = VirtualAlloc(base_ptr, size, MEM_RESERVE, PAGE_NOACCESS);
	if (!mem) throw std::system_error(GetLastError(), std::system_category());
	return mem;
}

void LocalMemoryManager::release(void * mem, std::size_t size)
{
	if (!VirtualFree(mem, 0, MEM_RELEASE))
		throw std::system_error(GetLastError(), std::system_category());
}

void LocalMemoryManager::commit(void * mem, std::size_t size)
{
	if (!VirtualAlloc(mem, size, MEM_COMMIT, PAGE_READWRITE))
		throw std::system_error(GetLastError(), std::system_category());
}

void LocalMemoryManager::decommit(void * mem, std::size_t size)
{
	if (!VirtualFree(mem, size, MEM_DECOMMIT))
		throw std::system_error(GetLastError(), std::system_category());
}

void LocalMemoryManager::set_access(void * mem, std::size_t size, int access)
{
	DWORD old_protect;
	const DWORD mem_flags = memory_access_to_win32(access);
	if (!VirtualProtect(mem, size, mem_flags, &old_protect))
		throw std::system_error(GetLastError(), std::system_category());
}

std::size_t LocalMemoryManager::copy_from(const void * mem, std::size_t size, void * into_buffer)
{
	std::copy_n(static_cast<const char *>(mem), size, static_cast<char *>(into_buffer));
	return size;
}

std::size_t LocalMemoryManager::copy_into(const void * data, std::size_t size, void * into_mem)
{
	std::copy_n(static_cast<const char *>(data), size, static_cast<char *>(into_mem));
	return size;
}

void LocalMemoryManager::register_exception_handlers(const void * base, void * entries, std::size_t count)
{
	const DWORD entries_count = static_cast<DWORD>(count);
	const RUNTIME_FUNCTION * entries_ptr = static_cast<RUNTIME_FUNCTION *>(entries);
	if (!RtlAddFunctionTable(entries_ptr, entries_count, reinterpret_cast<std::uintptr_t>(base)))
		throw std::system_error(GetLastError(), std::system_category());
}

void LocalMemoryManager::deregister_exception_handlers(void * entries, std::size_t count)
{
	if (!RtlDeleteFunctionTable(static_cast<RUNTIME_FUNCTION *>(entries)))
		throw std::system_error(GetLastError(), std::system_category());
}

std::future<int> LocalMemoryManager::run_async(const void * mem, void * params)
{
	return std::async(reinterpret_cast<int (__stdcall *)(void *)>(mem), params);
}

}