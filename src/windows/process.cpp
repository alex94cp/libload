#include "memory.hpp"
#include <load/process.hpp>

#include <windows.h>

#include <cassert>
#include <memory>
#include <system_error>

namespace load {

using namespace detail;

class CurrentProcess final : public Process
{
public:
	virtual ProcessId process_id() const override;
	virtual ProcessHandle native_handle() override;
	virtual MemoryManager & memory_manager() override;
	virtual const MemoryManager & memory_manager() const override;

	virtual void register_exception_table(std::uintptr_t base_address,
	                                      void         * exception_table,
	                                      std::size_t    table_size) override;
	
	virtual void deregister_exception_table(void * exception_table) override;
};

class LocalProcess final : public Process
{
public:
	explicit LocalProcess(HANDLE handle);
	LocalProcess(LocalProcess && other);
	virtual ~LocalProcess();

	virtual ProcessId process_id() const override;
	virtual ProcessHandle native_handle() override;
	virtual MemoryManager & memory_manager() override;
	virtual const MemoryManager & memory_manager() const override;

	virtual void register_exception_table(std::uintptr_t base_address,
	                                      void         * exception_table,
	                                      std::size_t    table_size) override;
	
	virtual void deregister_exception_table(void * exception_table) override;

private:
	HANDLE             _handle;
	LocalProcessMemory _memory_manager;
};

namespace {
	CurrentProcessMemory current_process_memory;
}

Process & current_process()
{
	static CurrentProcess current_process;
	return current_process;
}

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

ProcessId CurrentProcess::process_id() const
{
	return GetCurrentProcessId();
}

ProcessHandle CurrentProcess::native_handle()
{
	return GetCurrentProcess();
}

const MemoryManager & CurrentProcess::memory_manager() const
{
	return current_process_memory;
}

MemoryManager & CurrentProcess::memory_manager()
{
	return current_process_memory;
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

LocalProcess::LocalProcess(HANDLE handle)
	: _handle { handle }
	, _memory_manager { handle }
{
	assert(handle != GetCurrentProcess());
}

LocalProcess::LocalProcess(LocalProcess && other)
	: LocalProcess { other._handle }
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

}