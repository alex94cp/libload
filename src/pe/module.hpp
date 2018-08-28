#ifndef LOAD_SRC_PE_MODULE_HPP_
#define LOAD_SRC_PE_MODULE_HPP_

#include "image.hpp"
#include "../memory_block.hpp"
#include "../module_provider.hpp"
#include <load/module/module.hpp>

#include <peplus/any_buffer.hpp>
#include <peplus/virtual_image.hpp>

#include <algorithm>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace load::detail {

template <unsigned int XX, class MemoryOwnership>
class PEBasicModule : public Module
{
public:
	using module_memory = MemoryBlock<MemoryOwnership>;

	PEBasicModule(module_memory module_memory,
	              ModuleCache   module_cache);

protected:
	virtual DataPtr get_data_address(std::string_view name) const override;
	virtual ProcPtr get_proc_address(std::string_view name) const override;

	const void * find_symbol(std::string_view name) const;

	MemoryBlock<MemoryOwnership>         _image_mem;
	peplus::VirtualImage<XX, any_buffer> _module_image;
	mutable ModuleCache                  _module_cache;
};

template <unsigned int XX>
class OwnedPEModule final : public PEBasicModule<XX, owned_memory>
{
public:
	OwnedPEModule(Process   & process,
	              void      * image_ptr,
	              std::size_t image_size,
	              ModuleCache module_cache);

	OwnedPEModule(OwnedPEModule && other);
	virtual ~OwnedPEModule();

private:
	Process * _process;
};

template <unsigned int XX>
class BorrowedPEModule final : public PEBasicModule<XX, borrowed_memory>
{
public:
	BorrowedPEModule(const Process  & process,
	                 void           * image_ptr,
	                 std::size_t      image_size,
	                 ModuleProvider & module_provider);
};

#ifdef LIBLOAD_ENABLE_FORMAT_PE64

	bool is_valid_pe_module_64(const MemoryBuffer & image_data);

	std::shared_ptr<OwnedPEModule<64>> load_pe_module_64(const MemoryBuffer & image_data,
	                                                     ModuleProvider     & module_provider,
	                                                     Process            & into_process);

#endif

#ifdef LIBLOAD_ENABLE_FORMAT_PE32

	bool is_valid_pe_module_32(const MemoryBuffer & image_data);

	std::shared_ptr<OwnedPEModule<32>> load_pe_module_32(const MemoryBuffer & image_data,
	                                                     ModuleProvider     & module_provider,
	                                                     Process            & into_process);

#endif

std::optional<std::pair<std::string, std::string>>
	parse_pe_forwarder_string(std::string_view fwd_string);

template <unsigned int XX, class MO>
PEBasicModule<XX, MO>::PEBasicModule(module_memory module_data,
                                     ModuleCache   module_cache)
	: _image_mem { std::move(module_data) }
	, _module_image { _image_mem }
	, _module_cache { std::move(module_cache) }
{}

template <unsigned int XX, class MO>
DataPtr PEBasicModule<XX, MO>::get_data_address(std::string_view name) const
{
	return reinterpret_cast<DataPtr>(find_symbol(name));
}

template <unsigned int XX, class MO>
ProcPtr PEBasicModule<XX, MO>::get_proc_address(std::string_view name) const
{
	return reinterpret_cast<ProcPtr>(find_symbol(name));
}

template <unsigned int XX, class MO>
const void * PEBasicModule<XX, MO>::find_symbol(std::string_view name) const
{
	const auto export_dir = _module_image.export_directory();
	if (!export_dir) return nullptr;

	const auto export_info = export_dir->find(name);
	if (!export_info) return nullptr;

	if (!export_info->is_forwarded) {
		return _image_mem.data() + export_info->address.value();
	} else {
		const std::string_view fwd_string = *export_info->forwarder_string;
		const auto fwd_string_parts = parse_pe_forwarder_string(fwd_string);
		if (!fwd_string_parts) return nullptr;

		const auto [fwd_name, fwd_sym] = *fwd_string_parts;
		const auto fwd_modsp = _module_cache.get_module(fwd_name);
		if (fwd_modsp == nullptr) return nullptr;

		return fwd_modsp->get_data<void>(fwd_sym);
	}
}

template <unsigned int XX>
OwnedPEModule<XX>::OwnedPEModule(Process   & process,
                                 void      * image_ptr,
                                 std::size_t image_size,
                                 ModuleCache module_cache)
	: PEBasicModule {
		OwnedMemoryBlock(process.memory_manager(), image_ptr, image_size),
		std::move(module_cache)
	  }
	, _process { &process }
{}

template <unsigned int XX>
OwnedPEModule<XX>::OwnedPEModule(OwnedPEModule && other)
	: PEBasicModule { std::move(other) }
	, _process { other._process }
{
	other._process = nullptr;
}

template <unsigned int XX>
OwnedPEModule<XX>::~OwnedPEModule()
{
	if (_process != nullptr)
		deinitialize_dll(_module_image, *_process, _image_mem);
}

template <unsigned int XX>
BorrowedPEModule<XX>::BorrowedPEModule(const Process  & process,
                                       void           * image_ptr,
                                       std::size_t      image_size,
                                       ModuleProvider & module_provider)
	: PEBasicModule<XX, borrowed_memory> {
		BorrowedMemoryBlock(process.memory_manager(), image_ptr, image_size),
		ModuleCache(module_provider)
	}
{}

}

#endif