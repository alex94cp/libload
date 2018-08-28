#include <config.hpp>
#include <load/module/load_module.hpp>

#if defined(LIBLOAD_ENABLE_FORMAT_PE64) || defined(LIBLOAD_ENABLE_FORMAT_PE32)
#	include "pe/module.hpp"
#endif

namespace load {

using namespace detail;

std::shared_ptr<Module> load_module(const MemoryBuffer & module_data,
                                    ModuleProvider     & module_provider,
                                    Process            & into_process)
{
#ifdef LIBLOAD_ENABLE_FORMAT_PE64
	if (is_valid_pe_module_64(module_data))
		return load_pe_module_64(module_data, module_provider, into_process);
#endif

#ifdef LIBLOAD_ENABLE_FORMAT_PE32
	if (is_valid_pe_module_32(module_data))
		return load_pe_module_32(module_data, module_provider, into_process);
#endif

	return nullptr;
}

}