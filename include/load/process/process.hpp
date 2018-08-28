#ifndef LOAD_PROCESS_PROCESS_HPP_
#define LOAD_PROCESS_PROCESS_HPP_

#include <load/export.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>

namespace load {

class CodeGenerator;
class MemoryManager;
class ModuleProvider;

using ProcessHandle = void *;
using ProcessId = unsigned long;

class LOAD_EXPORT Process
{
public:
	virtual ~Process() = default;
	virtual ProcessId process_id() const = 0;
	virtual ProcessHandle native_handle() = 0;

	virtual MemoryManager & memory_manager() = 0;
	virtual const MemoryManager & memory_manager() const = 0;
	
	virtual ModuleProvider & module_provider() const = 0;
	virtual const CodeGenerator & code_generator() const = 0;

	virtual void register_exception_table(std::uintptr_t base_address,
	                                      void         * exception_table,
	                                      std::size_t    table_size) = 0;
	
	virtual void deregister_exception_table(void * exception_table) = 0;
};

LOAD_EXPORT Process & current_process();
LOAD_EXPORT std::unique_ptr<Process> open_process(ProcessId process_id);

}

#endif