#ifndef LOAD_SRC_PE_MODULE_HPP_
#define LOAD_SRC_PE_MODULE_HPP_

#include <load/module.hpp>

namespace load::detail {

bool is_valid_pe_module_64(const MemoryBuffer & image_data);
bool is_valid_pe_module_32(const MemoryBuffer & image_data);

std::shared_ptr<Module> load_pe_module_64(const MemoryBuffer   & image_data,
                                          const ModuleProvider & module_provider,
                                          MemoryManager        & memory_manager);

std::shared_ptr<Module> load_pe_module_32(const MemoryBuffer   & image_data,
                                          const ModuleProvider & module_provider,
                                          MemoryManager        & memory_manager);

}

#endif