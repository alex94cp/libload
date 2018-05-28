#ifndef LOAD_MEMORYMODULE_HPP_
#define LOAD_MEMORYMODULE_HPP_

#include <load/memory.hpp>
#include <load/module.hpp>

#include <memory>

namespace load {

class MemoryModule : public Module
{
public:
	virtual const void * base() const = 0;
	virtual void notify_thread_init() = 0;
	virtual void notify_thread_exit() = 0;
};

LOAD_EXPORT std::shared_ptr<Module>
	load_module(const MemoryBuffer   & module_data,
	            const ModuleProvider & module_provider = system_module_provider,
	            MemoryManager        & memory_manager  = local_memory_manager);

}

#endif