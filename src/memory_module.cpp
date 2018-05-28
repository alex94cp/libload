#include <config.hpp>
#include <load/memory_module.hpp>

#if defined(LIBLOAD_ENABLE_PE64_SUPPORT) || defined(LIBLOAD_ENABLE_PE32_SUPPORT)
#	include "pe/module.hpp"
#endif

namespace load {

std::shared_ptr<Module> load_module(const MemoryBuffer   & module_data,
                                    const ModuleProvider & module_provider,
                                    MemoryManager        & memory_manager)
{
#ifdef LIBLOAD_ENABLE_PE64_SUPPORT
	if (detail::is_valid_pe_module_64(module_data))
		return detail::load_pe_module_64(module_data, module_provider, memory_manager);
#endif

#ifdef LIBLOAD_ENABLE_PE32_SUPPORT
	if (detail::is_valid_pe_module_32(module_data))
		return detail::load_pe_module_32(module_data, module_provider, memory_manager);
#endif

	return nullptr;
}

}