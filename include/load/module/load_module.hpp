#ifndef LOAD_MODULE_LOADMODULE_HPP_
#define LOAD_MODULE_LOADMODULE_HPP_

#include <load/module/module_provider.hpp>
#include <load/process/process.hpp>

#include <memory>

namespace load {

class MemoryBuffer;

LOAD_EXPORT
std::shared_ptr<Module> load_module(const MemoryBuffer & module_data,
                                    ModuleProvider     & module_provider = system_module_provider,
                                    Process            & into_process    = current_process());

}

#endif