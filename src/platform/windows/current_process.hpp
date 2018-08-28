#ifndef LOAD_SRC_PLATFORM_WINDOWS_CURRENTPROCESS_HPP_
#define LOAD_SRC_PLATFORM_WINDOWS_CURRENTPROCESS_HPP_

#include <load/process/process.hpp>

namespace load::detail {

class CurrentProcess final : public Process
{
public:
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
};

}

#endif