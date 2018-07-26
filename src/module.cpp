#include "module.hpp"

#if defined(LIBLOAD_ENABLE_PE64_SUPPORT) || defined(LIBLOAD_ENABLE_PE32_SUPPORT)
#	include "pe/module.hpp"
#endif

namespace load {

std::shared_ptr<Module> load_module(const MemoryBuffer   & module_data,
                                    const ModuleProvider & module_provider,
                                    Process              & into_process)
{
	using namespace detail;

#ifdef LIBLOAD_ENABLE_PE64_SUPPORT
	if (is_valid_pe_module_64(module_data))
		return load_pe_module_64(module_data, module_provider, into_process);
#endif

#ifdef LIBLOAD_ENABLE_PE32_SUPPORT
	if (is_valid_pe_module_32(module_data))
		return load_pe_module_32(module_data, module_provider, into_process);
#endif

	return nullptr;
}

}