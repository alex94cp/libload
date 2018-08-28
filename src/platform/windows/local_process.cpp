#include <config.hpp>
#include "memory_access.hpp"
#include "local_process.hpp"
#include "current_process.hpp"
#include "../../arch/code_generator.hpp"
#include <load/module/module_provider.hpp>

#ifdef LIBLOAD_ENABLE_ARCH_X86
#	include "../../arch/x86/code_generator.hpp"
#endif

#ifdef LIBLOAD_ENABLE_ARCH_X86_64
#	include "../../arch/x64/code_generator.hpp"
#endif

#include <tlhelp32.h>
#include <wow64apiset.h>

#include <cassert>
#include <stdexcept>
#include <system_error>

namespace load {

using namespace detail;

std::unique_ptr<Process> open_process(ProcessId process_id)
{
	if (process_id == GetCurrentProcessId())
		return std::make_unique<CurrentProcess>();
	
	const DWORD access = PROCESS_VM_OPERATION | PROCESS_CREATE_THREAD;
	const HANDLE process_handle = OpenProcess(access, FALSE, process_id);
	if (!process_handle)
		throw std::system_error(GetLastError(), std::system_category());
	
	return std::make_unique<LocalProcess>(process_handle);
}

namespace detail {

template <unsigned int XX>
class LocalProcessModuleProvider final : public ModuleProvider
{
public:
	explicit LocalProcessModuleProvider(LocalProcess & local_process, DWORD flags);

	virtual std::shared_ptr<Module> get_module(std::string_view name) override;

private:
	LocalProcess * _local_process;
	DWORD          _th32cs_flags;
};

std::unique_ptr<ModuleProvider> make_local_process_module_provider(LocalProcess & process,
                                                                   DWORD          machine)
{
	switch (machine) {
		case IMAGE_FILE_MACHINE_I386:
			return std::make_unique<LocalProcessModuleProvider<32>>(process, TH32CS_SNAPMODULE);
		
		case IMAGE_FILE_MACHINE_IA64:
		case IMAGE_FILE_MACHINE_AMD64:
			return std::make_unique<LocalProcessModuleProvider<64>>(process, TH32CS_SNAPMODULE);
	
		default:
			throw std::runtime_error("Unknown process architecture");
	}
}

LocalProcess::LocalProcess(HANDLE handle)
	: _handle { handle }
	, _memory_manager { handle }
{
	assert(handle != GetCurrentProcess());
	
	USHORT process_machine, native_machine;
	if (!IsWow64Process2(_handle, &process_machine, &native_machine))
		throw std::system_error(GetLastError(), std::system_category());

	switch (process_machine) {
		case IMAGE_FILE_MACHINE_UNKNOWN:
			_module_provider = make_local_process_module_provider(*this, native_machine);
			_code_generator = &native_code_generator();
			break;

#ifdef LIBLOAD_ENABLE_ARCH_X86
		case IMAGE_FILE_MACHINE_I386: {
			static const X86CodeGenerator x86_codegen;
			_module_provider = std::make_unique<LocalProcessModuleProvider<32>>(*this, TH32CS_SNAPMODULE32);
			_code_generator = &x86_codegen;
			break;
		}
#endif

#ifdef LIBLOAD_ENABLE_ARCH_X86_64
		case IMAGE_FILE_MACHINE_IA64:
		case IMAGE_FILE_MACHINE_AMD64: {
			static const X64CodeGenerator x64_codegen;
			_module_provider = std::make_unique<LocalProcessModuleProvider<64>>(*this, TH32CS_SNAPMODULE);
			_code_generator = &x64_codegen;
			break;
		}
#endif

		default:
			throw std::runtime_error("Unknown process architecture");
	}
}

LocalProcess::LocalProcess(LocalProcess && other)
	: _handle { other._handle }, _memory_manager { _handle }
	, _module_provider { std::move(other._module_provider) }
	, _code_generator { other._code_generator }
{
	other._handle = nullptr;
}

LocalProcess::~LocalProcess()
{
	if (_handle != nullptr)
		CloseHandle(_handle);
}

ProcessId LocalProcess::process_id() const
{
	return GetProcessId(_handle);
}

ProcessHandle LocalProcess::native_handle()
{
	return _handle;
}

MemoryManager & LocalProcess::memory_manager()
{
	return _memory_manager;
}

const MemoryManager & LocalProcess::memory_manager() const
{
	return _memory_manager;
}

const CodeGenerator & LocalProcess::code_generator() const
{
	assert(_code_generator != nullptr);

	return *_code_generator;
}

ModuleProvider & LocalProcess::module_provider() const
{
	assert(_module_provider != nullptr);

	return *_module_provider;
}

void LocalProcess::register_exception_table(std::uintptr_t base_address,
                                            void         * exception_table,
                                            std::size_t    table_size)
{
	// TODO: implement exception table registration
}

void LocalProcess::deregister_exception_table(void * exception_table)
{
	// TODO: implement exception table deregistration
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
	if (mem == nullptr)
		throw std::system_error(GetLastError(), std::system_category());
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
	DWORD old_access;
	const DWORD mem_flags = memory_access_to_win32(access);
	if (!VirtualProtectEx(_handle, mem, size, mem_flags, &old_access))
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

} }