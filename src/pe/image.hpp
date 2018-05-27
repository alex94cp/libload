#ifndef LOAD_SRC_PE_IMAGE_HPP_
#define LOAD_SRC_PE_IMAGE_HPP_

#include "helper.hpp"
#include "../memory.hpp"
#include <load/module.hpp>

#include <peplus/file_image.hpp>
#include <peplus/virtual_image.hpp>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <stdexcept>

namespace load::detail {

enum {
	DLL_PROCESS_DETACH = 0,
	DLL_PROCESS_ATTACH = 1,
	DLL_THREAD_ATTACH  = 2,
	DLL_THREAD_DETACH  = 3,
};

using peplus::DWORD;
using HINSTANCE = void *;

using DllMain = bool (__stdcall *)(HINSTANCE, DWORD, void *);
using TlsCallback = void (__stdcall *)(HINSTANCE, DWORD, void *);

class PEPlusMemoryBufferAdapter final : public peplus::MemoryBuffer
{
public:
	explicit PEPlusMemoryBufferAdapter(const load::MemoryBuffer & mem_buffer);

	virtual std::size_t read(std::size_t offset, std::size_t size,
	                         void * into_buffer) const override;

private:
	const load::MemoryBuffer * _mem_buffer;
};

template <class Image>
MemoryBlock allocate_pe_image(const Image & image, MemoryManager & memory_manager)
{
	const auto opt_header = image.optional_header();
	const std::size_t image_size = opt_header.size_of_image;
	const std::uintptr_t image_base = opt_header.image_base;

	void * const image_mem = memory_manager.allocate(image_base, image_size);
	return MemoryBlock(memory_manager, image_mem, image_size);
}

template <unsigned int XX>
void copy_image_headers(const peplus::FileImage<XX> & image, MemoryBlock & into_mem)
{
	using namespace peplus::literals::offset_literals;

	const auto opt_header = image.optional_header();
	std::vector<char> hdrdata_buf (opt_header.size_of_headers);
	image.read(0_offs, hdrdata_buf.size(), hdrdata_buf.data());

	MemoryManager & mem_manager = into_mem.memory_manager();
	mem_manager.commit(into_mem.data(), hdrdata_buf.size());
	into_mem.write(0, hdrdata_buf.size(), hdrdata_buf.data());
}

template <unsigned int XX>
void copy_image_headers_direct(const peplus::FileImage<XX> & image, MemoryBlock & into_mem)
{
	using namespace peplus::literals::offset_literals;

	MemoryManager & mem_manager = into_mem.memory_manager();
	assert(mem_manager.allows_direct_addressing());

	const auto opt_header = image.optional_header();
	const std::size_t hdrs_size = opt_header.size_of_headers;
	if (hdrs_size > into_mem.size())
		throw std::runtime_error("Malformed image headers");

	mem_manager.commit(into_mem.data(), hdrs_size);
	image.read(0_offs, hdrs_size, into_mem.data());
}

template <unsigned int XX>
void map_pe_image_sections(const peplus::FileImage<XX> & image, MemoryBlock & into_mem)
{
	std::vector<char> rdata_buf;
	MemoryManager & mem_manager = into_mem.memory_manager();
	for (const auto & sect_header : image.section_headers()) {
		rdata_buf.resize(sect_header.size_of_raw_data);
		const peplus::FileOffset rdata_offs { sect_header.pointer_to_raw_data };
		image.read(rdata_offs, rdata_buf.size(), rdata_buf.data());

		const std::size_t vdata_offs = sect_header.virtual_address;
		void * const outmem_ptr = into_mem.data() + vdata_offs;
		mem_manager.commit(outmem_ptr, sect_header.virtual_size);
		into_mem.write(vdata_offs, rdata_buf.size(), rdata_buf.data());
	}
}

template <unsigned int XX>
void map_pe_image_sections_direct(const peplus::FileImage<XX> & image, MemoryBlock & into_mem)
{
	MemoryManager & mem_manager = into_mem.memory_manager();
	assert(mem_manager.allows_direct_addressing());

	for (const auto & sect_header : image.section_headers()) {
		const peplus::FileOffset rdata_offs { sect_header.pointer_to_raw_data };
		const std::size_t vdata_offs = sect_header.virtual_address;

		char * const outmem_ptr = into_mem.data() + vdata_offs;
		const std::size_t rem_size = into_mem.size() - vdata_offs;
		if (rem_size < sect_header.size_of_raw_data)
			throw std::runtime_error("Invalid section header");

		mem_manager.commit(outmem_ptr, sect_header.virtual_size);
		image.read(rdata_offs, sect_header.size_of_raw_data, outmem_ptr);
	}
}

template <unsigned int XX, class RelocationEntry>
void apply_pe_relocation_entry(const peplus::VirtualImage<XX> & image,
                               const RelocationEntry & reloc_entry,
                               MemoryBlock & image_mem)
{
	const std::uintptr_t preferred_base = image.optional_header().image_base;
	const auto base_address = reinterpret_cast<std::uintptr_t>(image_mem.data());
	const std::ptrdiff_t base_diff = base_address - preferred_base;
	const std::size_t rel_offs = reloc_entry.address.value();

	switch (reloc_entry.type) {
		case peplus::REL_BASED_ABSOLUTE:
			break;

		case peplus::REL_BASED_HIGHLOW:
			modify_le_value_at<std::uint32_t>(image_mem, rel_offs,
				[=] (auto x) { return x + static_cast<std::int32_t>(base_diff); });
			break;

		case peplus::REL_BASED_DIR64:
			modify_le_value_at<std::uint64_t>(image_mem, rel_offs,
				[=] (auto x) { return x + static_cast<std::int64_t>(base_diff); });
			break;

		default:
			throw std::runtime_error("Unsupported relocation type");
	}
}

template <class Image>
void apply_pe_image_relocations(const Image & image, MemoryBlock & image_mem)
{
	for (const auto & base_reloc : image.base_relocations()) {
		for (const auto & reloc_entry : base_reloc.entries())
			apply_pe_relocation_entry(image, reloc_entry, image_mem);
	}
}

template <class ImportDescriptor>
void resolve_pe_imported_symbols(const ImportDescriptor & import_dtor,
                                 const Module           & module,
                                 MemoryBlock            & image_mem)
{
	auto thunks_it = import_dtor.thunks().begin();
	for (const auto & import_entry : import_dtor.entries()) {
		std::visit([&] (const auto & import_info) {
			if constexpr (peplus::is_unnamed_import_v<decltype(import_info)>) {
				throw std::runtime_error("Imports by ordinal are not supported");
			} else {
				const void * const import_addr = module.get_data<void>(import_info.name);
				if (!import_addr) throw std::runtime_error("Image has unresolved imports");

				const std::size_t thunk_rva = thunks_it->offset().value();
				write_le_value_into(import_addr, image_mem, thunk_rva);
				++thunks_it;
			}
		}, import_entry);
	}
}

template <class Image>
void resolve_pe_image_imports(const Image          & image,
                              const ModuleProvider & mod_provider,
                              MemoryBlock          & image_mem)
{
	for (const auto & import_dtor : image.import_descriptors()) {
		const std::string mod_name = import_dtor.name_str();
		const auto mod_sp = mod_provider.get_module(mod_name);
		if (!mod_sp) throw std::runtime_error("Image has unresolved imports");
		resolve_pe_imported_symbols(import_dtor, *mod_sp, image_mem);
	}
}

constexpr int pe_section_memory_access(int characteristics)
{
	int mem_access = 0;
	if (characteristics & peplus::SCN_MEM_READ)    mem_access |= MemAccessRead;
	if (characteristics & peplus::SCN_MEM_WRITE)   mem_access |= MemAccessWrite;
	if (characteristics & peplus::SCN_MEM_EXECUTE) mem_access |= MemAccessExecute;
	return mem_access;
}

template <class Image>
void apply_pe_memory_permissions(const Image & image, MemoryBlock & image_mem)
{
	MemoryManager & mem_manager = image_mem.memory_manager();
	for (const auto & sect_header : image.section_headers()) {
		const std::size_t vdata_offs = sect_header.virtual_address;
		void * const mem_ptr = image_mem.data() + vdata_offs;

		const int mem_access = pe_section_memory_access(sect_header.characteristics);
		mem_manager.set_access(mem_ptr, sect_header.virtual_size, mem_access);
	}
}

template <class Image>
void remove_pe_discardable_sections(const Image & image, MemoryBlock & image_mem)
{
	MemoryManager & mem_manager = image_mem.memory_manager();
	for (const auto & section_header : image.section_headers()) {
		if (section_header.characteristics & peplus::SCN_MEM_DISCARDABLE) {
			const std::size_t vdata_size = section_header.virtual_size;
			const std::size_t vdata_offs = section_header.virtual_address;
			void * const vdata_ptr = image_mem.data() + vdata_offs;
			mem_manager.decommit(vdata_ptr, vdata_size);
		}
	}
}

template <unsigned int XX>
void invoke_pe_tls_callbacks_direct(const peplus::VirtualImage<XX> & image,
                                    MemoryBlock & image_mem, DWORD fdw_reason,
                                    void * lpv_reserved = nullptr)
{
	assert(image_mem.memory_manager().allows_direct_addressing());

	const HINSTANCE hinstance = image_mem.data();
	if (const auto tls_dir = image.tls_directory()) {
		for (const auto fn_rva : tls_dir->callbacks()) {
			void * const fn_ptr = image_mem.data() + fn_rva.value();
			const auto tls_fn = reinterpret_cast<TlsCallback>(fn_ptr);
			tls_fn(hinstance, fdw_reason, lpv_reserved);
		}
	}
}

template <unsigned int XX>
void invoke_dll_entry_point_direct(const peplus::VirtualImage<XX> & image,
                                   MemoryBlock & image_mem, DWORD fdw_reason,
                                   void * lpv_reserved = nullptr)
{
	assert(image.type() == peplus::ImageType::Dynamic);
	assert(image_mem.memory_manager().allows_direct_addressing());

	const auto opt_header = image.optional_header();
	const std::size_t ep_rva = opt_header.address_of_entry_point;
	const auto dll_main = reinterpret_cast<DllMain>(image_mem.data() + ep_rva);

	const HINSTANCE hinstance = image_mem.data();
	if (!dll_main(hinstance, fdw_reason, lpv_reserved))
		throw std::runtime_error("Module initialization failed");
}

template <class Image>
const void * find_pe_exported_symbol(const Image & image,
                                     const MemoryBlock & image_mem,
                                     std::string_view name)
{
	const auto export_dir = image.export_directory();
	if (!export_dir) return nullptr;

	const auto export_info = export_dir->find(name);
	if (!export_info || export_info->is_forwarded) return nullptr;
	return image_mem.data() + export_info->address.value();
}

template <unsigned int XX>
int invoke_pe_trampoline_helper(const peplus::VirtualImage<XX> & image, MemoryBlock & image_mem,
                                const void * trampoline_ptr, MemoryBlock & params_mem,
                                const void * fn_ptr, DWORD fdw_reason, void * lpv_reserved)
{
	PEHelperParams helper_params;
	helper_params.fdw_reason = fdw_reason;
	helper_params.lpv_reserved = lpv_reserved;
	helper_params.hinstance = image_mem.data();
	helper_params.fwd_proc = reinterpret_cast<PEFwdProc>(fn_ptr);
	params_mem.write(0, sizeof(helper_params), &helper_params);

	MemoryManager & mem_manager = image_mem.memory_manager();
	return mem_manager.run_async(trampoline_ptr, params_mem.data()).get();
}

template <unsigned int XX>
void invoke_pe_tls_callbacks(const peplus::VirtualImage<XX> & image,
                             MemoryBlock & image_mem, const void * trampoline_ptr,
                             MemoryBlock & params_mem, DWORD fdw_reason,
                             void * lpv_reserved = nullptr)
{
	if (const auto tls_dir = image.tls_directory()) {
		for (const auto fn_rva : tls_dir->callbacks()) {
			void * const fn_ptr = image_mem.data() + fn_rva.value();
			invoke_pe_trampoline_helper(image, image_mem, trampoline_ptr, params_mem,
			                            fn_ptr, fdw_reason, lpv_reserved);
		}
	}
}

template <unsigned int XX>
void invoke_dll_entry_point(const peplus::VirtualImage<XX> & image,
                            MemoryBlock & image_mem, const void * trampoline_ptr,
                            MemoryBlock & params_mem, DWORD fdw_reason,
                            void * lpv_reserved = nullptr)
{
	assert(image.type() == peplus::ImageType::Dynamic);

	const auto opt_header = image.optional_header();
	const std::size_t ep_rva = opt_header.address_of_entry_point;
	const auto dll_main = image_mem.data() + opt_header.address_of_entry_point;
	const auto init_succeeded = invoke_pe_trampoline_helper(image, image_mem, trampoline_ptr,
	                                                        params_mem, dll_main, fdw_reason,
	                                                        lpv_reserved);

	if (!init_succeeded)
		throw std::runtime_error("Module initialization failed");
}

inline MemoryBlock allocate_pe_trampoline_params(MemoryBlock & image_mem)
{
	MemoryManager & mem_manager = image_mem.memory_manager();
	void * const params_ptr = mem_manager.allocate(0, sizeof(PEHelperParams));
	MemoryBlock params_mem { mem_manager, params_ptr, sizeof(PEHelperParams) };
	mem_manager.commit(params_mem.data(), sizeof(PEHelperParams));
	return params_mem;
}

template <class Image>
const void * get_pe_trampoline_address(const Image & image, const MemoryBlock & image_mem)
{
	const auto trampoline_ptr = find_pe_exported_symbol(image, image_mem, "__libload_helper");
	if (!trampoline_ptr) {
		throw std::runtime_error("Image initialization without direct addressing "
		                         "requires an exported function trampoline but "
		                         "it was not found");
	}

	return trampoline_ptr;
}

template <class Image>
void initialize_pe_image(const Image & image, MemoryBlock & image_mem)
{
	if (image.type() != peplus::ImageType::Dynamic)
		throw std::runtime_error("Image type not supported");

	if (image_mem.memory_manager().allows_direct_addressing()) {
		invoke_pe_tls_callbacks_direct(image, image_mem, DLL_PROCESS_ATTACH);
		invoke_pe_tls_callbacks_direct(image, image_mem, DLL_THREAD_ATTACH);

		invoke_dll_entry_point_direct(image, image_mem, DLL_PROCESS_ATTACH);
		invoke_dll_entry_point_direct(image, image_mem, DLL_THREAD_ATTACH);
	} else {
		MemoryBlock params_mem = allocate_pe_trampoline_params(image_mem);
		const auto trampoline_ptr = get_pe_trampoline_address(image, image_mem);

		invoke_pe_tls_callbacks(image, image_mem, trampoline_ptr, params_mem, DLL_PROCESS_ATTACH);
		invoke_pe_tls_callbacks(image, image_mem, trampoline_ptr, params_mem, DLL_THREAD_ATTACH);

		invoke_dll_entry_point(image, image_mem, trampoline_ptr, params_mem, DLL_PROCESS_ATTACH);
		invoke_dll_entry_point(image, image_mem, trampoline_ptr, params_mem, DLL_THREAD_ATTACH);
	}
}

template <class Image>
void deinitialize_pe_image(const Image & image, MemoryBlock & image_mem)
{
	if (image.type() != peplus::ImageType::Dynamic)
		throw std::runtime_error("Image type not supported");

	if (image_mem.memory_manager().allows_direct_addressing()) {
		invoke_pe_tls_callbacks_direct(image, image_mem, DLL_THREAD_DETACH);
		invoke_pe_tls_callbacks_direct(image, image_mem, DLL_PROCESS_DETACH);

		invoke_dll_entry_point_direct(image, image_mem, DLL_THREAD_DETACH);
		invoke_dll_entry_point_direct(image, image_mem, DLL_PROCESS_DETACH);
	} else {
		MemoryBlock params_mem = allocate_pe_trampoline_params(image_mem);
		const auto trampoline_ptr = get_pe_trampoline_address(image, image_mem);

		invoke_pe_tls_callbacks(image, image_mem, trampoline_ptr, params_mem, DLL_THREAD_DETACH);
		invoke_pe_tls_callbacks(image, image_mem, trampoline_ptr, params_mem, DLL_PROCESS_DETACH);

		invoke_dll_entry_point(image, image_mem, trampoline_ptr, params_mem, DLL_THREAD_DETACH);
		invoke_dll_entry_point(image, image_mem, trampoline_ptr, params_mem, DLL_PROCESS_DETACH);
	}
}

template <unsigned int XX>
MemoryBlock load_pe_image(const MemoryBuffer   & image_data,
                          MemoryManager        & mem_manager,
                          const ModuleProvider & mod_provider)
{
	const PEPlusMemoryBufferAdapter src_buf { image_data };
	const peplus::FileImage<XX> src_image { src_buf };
	auto image_mem = detail::allocate_pe_image(src_image, mem_manager);
	if (mem_manager.allows_direct_addressing()) {
		detail::copy_image_headers_direct(src_image, image_mem);
		detail::map_pe_image_sections_direct(src_image, image_mem);
	} else {
		detail::copy_image_headers(src_image, image_mem);
		detail::map_pe_image_sections(src_image, image_mem);
	}

	const PEPlusMemoryBufferAdapter dst_buf { image_mem };
	const peplus::VirtualImage<XX> dst_image { dst_buf };
	detail::apply_pe_image_relocations(dst_image, image_mem);
	detail::resolve_pe_image_imports(dst_image, mod_provider, image_mem);
	detail::apply_pe_memory_permissions(dst_image, image_mem);
	detail::remove_pe_discardable_sections(dst_image, image_mem);
	detail::initialize_pe_image(dst_image, image_mem);
	return image_mem;
}

}

#endif