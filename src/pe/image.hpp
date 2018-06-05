#ifndef LOAD_SRC_PE_IMAGE_HPP_
#define LOAD_SRC_PE_IMAGE_HPP_

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

template <class PEImage>
MemoryBlock allocate_pe_image(const PEImage & image, MemoryManager & memory_manager)
{
	const auto opt_header = image.optional_header();
	const std::size_t image_size = opt_header.size_of_image;
	const std::uintptr_t image_base = opt_header.image_base;

	void * const image_mem = memory_manager.allocate(image_base, image_size);
	return MemoryBlock(memory_manager, image_mem, image_size);
}

template <class PEFileImage>
void copy_pe_image_headers_indirect(const PEFileImage & image, MemoryBlock & into_mem)
{
	using namespace peplus::literals::offset_literals;

	const auto opt_header = image.optional_header();
	std::vector<char> hdrdata_buf (opt_header.size_of_headers);
	image.read(0_offs, hdrdata_buf.size(), hdrdata_buf.data());

	MemoryManager & mem_manager = into_mem.memory_manager();
	mem_manager.commit(into_mem.data(), hdrdata_buf.size());
	into_mem.write(0, hdrdata_buf.data(), hdrdata_buf.size());
}

template <class PEFileImage>
void copy_pe_image_headers_direct(const PEFileImage & image, MemoryBlock & into_mem)
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

template <class PEFileImage>
void map_pe_image_sections_indirect(const PEFileImage & image, MemoryBlock & into_mem)
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
		into_mem.write(vdata_offs, rdata_buf.data(), rdata_buf.size());
	}
}

template <class PEFileImage>
void map_pe_image_sections_direct(const PEFileImage & image, MemoryBlock & into_mem)
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

template <class PEVirtualImage, class RelocationEntry>
void apply_pe_relocation_entry(const PEVirtualImage & image,
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

template <class PEImage>
void apply_pe_image_relocations(const PEImage & image, MemoryBlock & image_mem)
{
	for (const auto & base_reloc : image.base_relocations()) {
		for (const auto & reloc_entry : base_reloc.entries())
			apply_pe_relocation_entry(image, reloc_entry, image_mem);
	}
}

template <class PEImportDescriptor>
void resolve_pe_imported_symbols(const PEImportDescriptor & import_dtor,
                                 const Module             & module,
                                 MemoryBlock              & image_mem)
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

template <class PEImage>
void resolve_pe_image_imports(const PEImage        & image,
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

template <class PEImage>
void apply_pe_memory_permissions(const PEImage & image, MemoryBlock & image_mem)
{
	MemoryManager & mem_manager = image_mem.memory_manager();
	for (const auto & sect_header : image.section_headers()) {
		const std::size_t vdata_offs = sect_header.virtual_address;
		void * const mem_ptr = image_mem.data() + vdata_offs;

		const int mem_access = pe_section_memory_access(sect_header.characteristics);
		mem_manager.set_access(mem_ptr, sect_header.virtual_size, mem_access);
	}
}

template <class PEImage>
void remove_pe_discardable_sections(const PEImage & image, MemoryBlock & image_mem)
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
MemoryBlock load_pe_image(const MemoryBuffer   & image_data,
                          MemoryManager        & mem_manager,
                          const ModuleProvider & mod_provider)
{
	const peplus::FileImage<XX, any_buffer> src_image { image_data };
	auto image_mem = detail::allocate_pe_image(src_image, mem_manager);
	if (mem_manager.allows_direct_addressing()) {
		detail::copy_pe_image_headers_direct(src_image, image_mem);
		detail::map_pe_image_sections_direct(src_image, image_mem);
	} else {
		detail::copy_pe_image_headers_indirect(src_image, image_mem);
		detail::map_pe_image_sections_indirect(src_image, image_mem);
	}

	const peplus::VirtualImage<XX, any_buffer> dst_image { image_mem };
	detail::apply_pe_image_relocations(dst_image, image_mem);
	detail::resolve_pe_image_imports(dst_image, mod_provider, image_mem);
	detail::apply_pe_memory_permissions(dst_image, image_mem);
	detail::remove_pe_discardable_sections(dst_image, image_mem);
	return image_mem;
}

template <class PEVirtualImage>
void invoke_pe_tls_callbacks_direct(const PEVirtualImage & image,
                                    void * image_base, DWORD event,
                                    void * reserved)
{
	char * const image_ptr = static_cast<char *>(image_base);
	if (const auto tls_dir = image.tls_directory()) {
		for (const auto fn_rva : tls_dir->callbacks()) {
			void * const fn_ptr = image_ptr + fn_rva.value();
			const auto tls_fn = reinterpret_cast<TlsCallback>(fn_ptr);
			tls_fn(image_base, event, reserved);
		}
	}
}

template <class PEVirtualImage>
int invoke_dll_entry_point_direct(const PEVirtualImage & image,
                                  void * image_base, DWORD event,
                                  void * reserved)
{
	assert(image.type() == peplus::ImageType::Dynamic);

	const auto opt_header = image.optional_header();
	char * const image_ptr = static_cast<char *>(image_base);
	const std::size_t ep_rva = opt_header.address_of_entry_point;
	const auto dll_main = reinterpret_cast<DllMain>(image_ptr + ep_rva);
	return dll_main(image_base, event, reserved);
}

template <class PEImage>
int notify_dll_event_direct(const PEImage & image, void * image_base,
                            DWORD event, void * reserved = nullptr)
{
	invoke_pe_tls_callbacks_direct(image, image_base, event, reserved);
	return invoke_dll_entry_point_direct(image, image_base, event, reserved);
}

template <class PEImage>
bool initialize_dll_direct(const PEImage & image, void * image_base)
{
	if (!notify_dll_event_direct(image, image_base, DLL_PROCESS_ATTACH))
		return false;

	notify_dll_event_direct(image, image_base, DLL_THREAD_ATTACH);
	return true;
}

template <class PEImage>
const void * find_pe_exported_symbol(const PEImage & image,
                                     const MemoryBlock & image_mem,
                                     std::string_view name)
{
	const auto export_dir = image.export_directory();
	if (!export_dir) return nullptr;

	const auto export_info = export_dir->find(name);
	if (!export_info || export_info->is_forwarded) return nullptr;
	return image_mem.data() + export_info->address.value();
}

template <class PEImage>
const void * get_dll_init_helper(const PEImage & image, const MemoryBlock & image_mem)
{
	const void * const helper = find_pe_exported_symbol(image, image_mem, "__libload_init");
	if (!helper) {
		throw std::runtime_error("DLL initialization without direct addressing "
		                         "requires an exported function helper named "
		                         "__libload_init but it wasn't found");
	}

	return helper;
}

template <class PEImage>
bool initialize_dll(const PEImage & image, MemoryBlock & image_mem)
{
	MemoryManager & mem_manager = image_mem.memory_manager();
	if (mem_manager.allows_direct_addressing()) {
		return initialize_dll_direct(image, image_mem.data());
	} else {
		const void * const init_helper = get_dll_init_helper(image, image_mem);
		auto init_task = mem_manager.run_async(init_helper, image_mem.data());
		return init_task.get() >= 0;
	}
}

template <class PEImage>
const void * get_dll_cleanup_helper(const PEImage & image, const MemoryBlock & image_mem)
{
	const void * const helper = find_pe_exported_symbol(image, image_mem, "__libload_cleanup");
	if (!helper) {
		throw std::runtime_error("DLL deinitialization without direct addressing "
		                         "requires an exported function helper named "
		                         "__libload_cleanup but it wasn't found");
	}

	return helper;
}

template <class PEImage>
void deinitialize_dll_direct(const PEImage & image, void * image_base)
{
	notify_dll_event_direct(image, image_base, DLL_THREAD_DETACH);
	notify_dll_event_direct(image, image_base, DLL_PROCESS_DETACH);
}

template <class PEImage>
void deinitialize_dll(const PEImage & image, MemoryBlock & image_mem)
{
	MemoryManager & mem_manager = image_mem.memory_manager();
	if (mem_manager.allows_direct_addressing()) {
		deinitialize_dll_direct(image, image_mem.data());
	} else {
		const void * const deinit_helper = get_dll_cleanup_helper(image, image_mem);
		mem_manager.run_async(deinit_helper, image_mem.data()).wait();
	}
}

}

#endif