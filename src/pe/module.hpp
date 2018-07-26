#ifndef LOAD_SRC_PE_MODULE_HPP_
#define LOAD_SRC_PE_MODULE_HPP_

#include "../module.hpp"

namespace load::detail {

#ifdef LIBLOAD_ENABLE_PE64_SUPPORT

	bool is_valid_pe_module_64(const MemoryBuffer & image_data);

	std::shared_ptr<Module> load_pe_module_64(const MemoryBuffer   & image_data,
	                                          const ModuleProvider & module_provider,
	                                          Process              & into_process);

#endif

#ifdef LIBLOAD_ENABLE_PE32_SUPPORT

	bool is_valid_pe_module_32(const MemoryBuffer & image_data);

	std::shared_ptr<Module> load_pe_module_32(const MemoryBuffer   & image_data,
	                                          const ModuleProvider & module_provider,
	                                          Process              & into_process);

#endif

}

#endif