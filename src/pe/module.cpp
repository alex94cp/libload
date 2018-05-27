#include "module.hpp"
#include "image.hpp"

#include <algorithm>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>

namespace load::detail {

template <unsigned int XX>
class PEModule final : public Module
{
public:
	explicit PEModule(const MemoryBuffer   & image_data,
	                  MemoryManager        & mem_manager,
	                  const ModuleProvider & mod_provider);
	virtual ~PEModule();

protected:
	virtual DataPtr get_data_address(std::string_view name) const override;
	virtual ProcPtr get_proc_address(std::string_view name) const override;

private:
	const void * find_symbol(std::string_view name) const;

	std::unordered_map<std::string, std::shared_ptr<Module>> _module_deps;
	MemoryBlock                                              _image_mem;
	PEPlusMemoryBufferAdapter                                _image_buf;
	peplus::VirtualImage<XX>                                 _image;
};

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

template <unsigned int XX>
bool is_valid_pe_module(const MemoryBuffer & image_data)
{
	const PEPlusMemoryBufferAdapter buf_adapter { image_data };
	return peplus::FileImage<XX>::is_valid(buf_adapter);
}

bool is_valid_pe_module_64(const MemoryBuffer & image_data)
{
	return is_valid_pe_module<64>(image_data);
}

bool is_valid_pe_module_32(const MemoryBuffer & image_data)
{
	return is_valid_pe_module<32>(image_data);
}

template <unsigned int XX>
std::shared_ptr<Module> load_pe_module(const MemoryBuffer   & image_data,
                                       const ModuleProvider & mod_provider,
                                       MemoryManager        & mem_manager)
{
	return std::make_shared<PEModule<XX>>(image_data, mem_manager, mod_provider);
}

std::shared_ptr<Module> load_pe_module_64(const MemoryBuffer   & image_data,
                                          const ModuleProvider & mod_provider,
                                          MemoryManager        & mem_manager)
{
	return load_pe_module<64>(image_data, mod_provider, mem_manager);
}

std::shared_ptr<Module> load_pe_module_32(const MemoryBuffer   & image_data,
                                          const ModuleProvider & mod_provider,
                                          MemoryManager        & mem_manager)
{
	return load_pe_module<32>(image_data, mod_provider, mem_manager);
}

template <unsigned int XX>
PEModule<XX>::PEModule(const MemoryBuffer   & image_data,
                       MemoryManager        & mem_manager,
                       const ModuleProvider & mod_provider)
	: _image_mem { [&] {
		return load_pe_image<XX>(image_data, mem_manager,
			make_module_provider([&] (std::string_view name) {
				auto mod_sp = mod_provider.get_module(name);
				if (mod_sp)
					_module_deps.emplace(name, mod_sp);
				return mod_sp;
			}));
		}() }
	, _image_buf { _image_mem }
	, _image { _image_buf }
{}

template <unsigned int XX>
PEModule<XX>::~PEModule()
{
	detail::deinitialize_pe_image(_image, _image_mem);
}

template <unsigned int XX>
DataPtr PEModule<XX>::get_data_address(std::string_view name) const
{
	return find_symbol(name);
}

template <unsigned int XX>
ProcPtr PEModule<XX>::get_proc_address(std::string_view name) const
{
	return reinterpret_cast<ProcPtr>(find_symbol(name));
}

template <unsigned int XX>
const void * PEModule<XX>::find_symbol(std::string_view name) const
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