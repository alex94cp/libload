#include "module.hpp"
#include "image.hpp"

#include <algorithm>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace load::detail {

template <unsigned int XX, class ModuleOwnershipPolicy>
class PEModule final : public Module
{
public:
	template <class ModuleMap>
	explicit PEModule(Process         & process,
	                  void            * image_mem,
	                  std::size_t       image_size,
	                  const ModuleMap & module_deps);
	virtual ~PEModule();

protected:
	virtual DataPtr get_data_address(std::string_view name) const override;
	virtual ProcPtr get_proc_address(std::string_view name) const override;

private:
	const void * find_symbol(std::string_view name) const;

	Process                                                  * _process;
	MemoryBlock<typename ModuleOwnershipPolicy::memory_policy> _image_mem;
	peplus::VirtualImage<XX, any_buffer>                       _image;
	std::unordered_map<std::string, std::shared_ptr<Module>>   _module_deps;
};

template <unsigned int XX>
std::shared_ptr<Module> load_pe_module(const MemoryBuffer   & image_data,
                                       const ModuleProvider & module_provider,
                                       Process              & into_process)
{
	using PEModuleResult = PEModule<XX, owned_module>;

	MemoryManager & mem_manager = into_process.memory_manager();
	CachingModuleProvider<> caching_mod_provider { module_provider };
	OwnedMemoryBlock image_mem = load_pe_image<XX>(image_data, mem_manager, caching_mod_provider);
	initialize_dll(peplus::VirtualImage<XX, any_buffer>(image_mem), into_process, image_mem);
	auto module = std::make_shared<PEModuleResult>(into_process, image_mem.data(), image_mem.size(),
	                                               caching_mod_provider.module_cache());
	image_mem.release();
	return module;
}

#ifdef LIBLOAD_ENABLE_PE64_SUPPORT

bool is_valid_pe_module_64(const MemoryBuffer & image_data)
{
	return peplus::FileImage64<any_buffer>::is_valid(image_data);
}

std::shared_ptr<Module> load_pe_module_64(const MemoryBuffer   & image_data,
                                          const ModuleProvider & mod_provider,
                                          Process              & into_process)
{
	return load_pe_module<64>(image_data, mod_provider, into_process);
}

#endif

#ifdef LIBLOAD_ENABLE_PE32_SUPPORT

bool is_valid_pe_module_32(const MemoryBuffer & image_data)
{
	return peplus::FileImage32<any_buffer>::is_valid(image_data);
}

std::shared_ptr<Module> load_pe_module_32(const MemoryBuffer   & image_data,
                                          const ModuleProvider & mod_provider,
                                          Process              & into_process)
{
	return load_pe_module<32>(image_data, mod_provider, into_process);
}

#endif

namespace {

std::optional<std::pair<std::string, std::string>>
	parse_forwarder_string(std::string_view fwd_string)
{
	const std::size_t dot_pos = fwd_string.find('.');
	if (dot_pos == std::string::npos) return std::nullopt;

	std::string mod_name { fwd_string.substr(0, dot_pos) };
	std::string proc_name { fwd_string.substr(dot_pos + 1) };
	return std::pair(std::move(mod_name), std::move(proc_name));
}

}

template <unsigned int XX, class MOP> template <class ModuleMap>
PEModule<XX, MOP>::PEModule(Process & process,
                            void * mem, std::size_t size,
                            const ModuleMap & module_deps)
	: _process { &process }, _image_mem { process.memory_manager(), mem, size }
	, _image { _image_mem }, _module_deps { module_deps.begin(), module_deps.end() }
{}

template <unsigned int XX, class MOP>
PEModule<XX, MOP>::~PEModule()
{
	MOP::dispose([this] {
		detail::deinitialize_dll(_image, *_process, _image_mem);
	});
}

template <unsigned int XX, class MOP>
DataPtr PEModule<XX, MOP>::get_data_address(std::string_view name) const
{
	return reinterpret_cast<DataPtr>(find_symbol(name));
}

template <unsigned int XX, class MOP>
ProcPtr PEModule<XX, MOP>::get_proc_address(std::string_view name) const
{
	return reinterpret_cast<ProcPtr>(find_symbol(name));
}

template <unsigned int XX, class MOP>
const void * PEModule<XX, MOP>::find_symbol(std::string_view name) const
{
	const auto export_dir = _image.export_directory();
	if (!export_dir) return nullptr;

	const auto export_info = export_dir->find(name);
	if (!export_info) return nullptr;

	if (!export_info->is_forwarded) {
		return _image_mem.data() + export_info->address.value();
	} else {
		const std::string_view fwd_string = *export_info->forwarder_string;
		const auto fwd_string_parts = parse_forwarder_string(fwd_string);
		if (!fwd_string_parts) return nullptr;

		const auto [fwd_modname, fwd_sym] = *fwd_string_parts;
		const auto fwd_modsp_it = _module_deps.find(fwd_modname);
		if (fwd_modsp_it == _module_deps.end()) return nullptr;

		return fwd_modsp_it->second->get_data<void>(fwd_sym);
	}
}

}