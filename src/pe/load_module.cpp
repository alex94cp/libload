#include "image.hpp"
#include "module.hpp"

namespace load::detail {

template <unsigned int XX>
std::shared_ptr<OwnedPEModule<XX>> load_pe_module(const MemoryBuffer & image_data,
                                                  ModuleProvider     & module_provider,
                                                  Process            & into_process)
{
	ModuleCache module_cache { module_provider };
	MemoryManager & mem_manager = into_process.memory_manager();
	OwnedMemoryBlock image_mem = load_pe_image<XX>(image_data, mem_manager, module_cache);
	initialize_dll(peplus::VirtualImage<XX, any_buffer>(image_mem), into_process, image_mem);

	auto module = std::make_shared<OwnedPEModule<XX>>(into_process, image_mem.data(),
	                                                  image_mem.size(), std::move(module_cache));
	image_mem.release();
	return module;
}

#ifdef LIBLOAD_ENABLE_FORMAT_PE64

bool is_valid_pe_module_64(const MemoryBuffer & image_data)
{
	return peplus::FileImage64<any_buffer>::is_valid(image_data);
}

std::shared_ptr<OwnedPEModule<64>> load_pe_module_64(const MemoryBuffer & image_data,
                                                     ModuleProvider     & mod_provider,
                                                     Process            & into_process)
{
	return load_pe_module<64>(image_data, mod_provider, into_process);
}

#endif

#ifdef LIBLOAD_ENABLE_FORMAT_PE32

bool is_valid_pe_module_32(const MemoryBuffer & image_data)
{
	return peplus::FileImage32<any_buffer>::is_valid(image_data);
}

std::shared_ptr<OwnedPEModule<32>> load_pe_module_32(const MemoryBuffer & image_data,
                                                     ModuleProvider     & mod_provider,
                                                     Process            & into_process)
{
	return load_pe_module<32>(image_data, mod_provider, into_process);
}

#endif

std::optional<std::pair<std::string, std::string>>
	parse_pe_forwarder_string(std::string_view fwd_string)
{
	const std::size_t dot_pos = fwd_string.find('.');
	if (dot_pos == std::string::npos) return std::nullopt;

	std::string mod_name { fwd_string.substr(0, dot_pos) };
	std::string proc_name { fwd_string.substr(dot_pos + 1) };
	return std::pair(std::move(mod_name), std::move(proc_name));
}

}