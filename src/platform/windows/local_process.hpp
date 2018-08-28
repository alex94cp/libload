#ifndef LOAD_SRC_PLATFORM_WINDOWS_LOCALPROCESS_HPP_
#define LOAD_SRC_PLATFORM_WINDOWS_LOCALPROCESS_HPP_

#include <load/process/process.hpp>
#include <load/memory/memory_manager.hpp>

#include <windows.h>

namespace load::detail {

class load::CodeGenerator;

class LocalProcessMemory final : public MemoryManager
{
public:
	explicit LocalProcessMemory(HANDLE handle);

	virtual bool allows_direct_addressing() const override;

	virtual void * allocate(std::uintptr_t base, std::size_t size) override;
	virtual void set_access(void * mem, std::size_t size, int access) override;
	virtual void release(void * mem, std::size_t size) override;

	virtual std::size_t copy_from(const void * mem, std::size_t size, void * into_buffer) override;
	virtual std::size_t copy_into(const void * data, std::size_t size, void * into_mem) override;

	virtual void commit(void * mem, std::size_t size) override;
	virtual void decommit(void * mem, std::size_t size) override;

private:
	HANDLE _handle;
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
	
	virtual ModuleProvider & module_provider() const override;
	virtual const CodeGenerator & code_generator() const override;

	virtual void register_exception_table(std::uintptr_t base_address,
	                                      void         * exception_table,
	                                      std::size_t    table_size) override;
	
	virtual void deregister_exception_table(void * exception_table) override;

private:
	HANDLE                          _handle;
	const CodeGenerator           * _code_generator;
	LocalProcessMemory              _memory_manager;
	std::unique_ptr<ModuleProvider> _module_provider;
};

}

#endif