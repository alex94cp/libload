#include <load/memory/memory_manager.hpp>
#include <load/module/module_provider.hpp>

#include "system_module.hpp"
#include "memory_access.hpp"
#include "current_process.hpp"
#include "../../arch/code_generator.hpp"

#include <memory>
#include <system_error>

namespace load {

using namespace detail;

Process & current_process()
{
	static CurrentProcess current_process;
	return current_process;
}

namespace detail {

class CurrentProcessMemory final : public MemoryManager
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
};

class CurrentProcessModuleProvider final : public ModuleProvider
{
public:
	virtual std::shared_ptr<Module> get_module(std::string_view name) override;
};

ProcessId CurrentProcess::process_id() const
{
	return GetCurrentProcessId();
}

ProcessHandle CurrentProcess::native_handle()
{
	return GetCurrentProcess();
}

namespace {
	CurrentProcessMemory current_process_memory;
}

const MemoryManager & CurrentProcess::memory_manager() const
{
	return current_process_memory;
}

MemoryManager & CurrentProcess::memory_manager()
{
	return current_process_memory;
}

const CodeGenerator & CurrentProcess::code_generator() const
{
	return native_code_generator();
}

ModuleProvider & CurrentProcess::module_provider() const
{
	static CurrentProcessModuleProvider module_provider;
	return module_provider;
}

void CurrentProcess::register_exception_table(std::uintptr_t base_address,
                                              void         * exception_table,
                                              std::size_t    table_size)
{
	const std::size_t entry_count = table_size / sizeof(RUNTIME_FUNCTION);
	const auto rftable_ptr = static_cast<PRUNTIME_FUNCTION>(exception_table);
	if (!RtlAddFunctionTable(rftable_ptr, static_cast<DWORD>(entry_count), base_address))
		throw std::system_error(GetLastError(), std::system_category());
}

void CurrentProcess::deregister_exception_table(void * exception_table)
{
	const auto rftable_ptr = static_cast<PRUNTIME_FUNCTION>(exception_table);
	if (!RtlDeleteFunctionTable(rftable_ptr))
		throw std::system_error(GetLastError(), std::system_category());
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

std::shared_ptr<Module> CurrentProcessModuleProvider::get_module(std::string_view name)
{
	HMODULE handle;
	const std::string name_s { name };
	if (!GetModuleHandleExA(0, name_s.c_str(), &handle))
		throw std::system_error(GetLastError(), std::system_category());
	
	return handle ? std::make_shared<SystemModule>(handle) : nullptr;
}

} }