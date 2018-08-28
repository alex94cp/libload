#ifndef LOAD_SRC_PE_IMAGE_HPP_
#define LOAD_SRC_PE_IMAGE_HPP_

#include "../memory_block.hpp"

#include <load/codegen/code_chunk.hpp>
#include <load/codegen/code_generator.hpp>
#include <load/module/module.hpp>
#include <load/module/module_provider.hpp>
#include <load/process/process.hpp>

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
OwnedMemoryBlock allocate_pe_image(const PEImage & image, MemoryManager & memory_manager)
{
	const auto opt_header = image.optional_header();
	const std::size_t image_size = opt_header.size_of_image;
	const std::uintptr_t image_base = opt_header.image_base;

	void * const image_mem = memory_manager.allocate(image_base, image_size);
	return OwnedMemoryBlock(memory_manager, image_mem, image_size);
}

template <class PEFileImage, class MemoryBlock>
void copy_pe_image_headers_indirect(const PEFileImage & image, MemoryBlock & into_memory)
{
	using namespace peplus::literals::offset_literals;

	const auto opt_header = image.optional_header();
	std::vector<char> hdrdata_buf (opt_header.size_of_headers);
	image.read(0_offs, hdrdata_buf.size(), hdrdata_buf.data());

	into_memory.memory_manager().commit(into_memory.data(), hdrdata_buf.size());
	into_memory.write(0, hdrdata_buf.data(), hdrdata_buf.size());
}

template <class PEFileImage, class MemoryBlock>
void copy_pe_image_headers_direct(const PEFileImage & image, MemoryBlock & into_memory)
{
	using namespace peplus::literals::offset_literals;

	assert(into_memory.memory_manager().allows_direct_addressing());

	const auto opt_header = image.optional_header();
	const std::size_t hdrs_size = opt_header.size_of_headers;
	if (hdrs_size > into_memory.size())
		throw std::runtime_error("Malformed image headers");

	into_memory.memory_manager().commit(into_memory.data(), hdrs_size);
	image.read(0_offs, hdrs_size, into_memory.data());
}

template <class PEFileImage, class MemoryBlock>
void map_pe_image_sections_indirect(const PEFileImage & image, MemoryBlock & into_memory)
{
	std::vector<char> rdata_buf;
	MemoryManager & memory_manager = into_memory.memory_manager();
	for (const auto & sect_header : image.section_headers()) {
		rdata_buf.resize(sect_header.size_of_raw_data);
		const peplus::FileOffset rdata_offs { sect_header.pointer_to_raw_data };
		image.read(rdata_offs, rdata_buf.size(), rdata_buf.data());

		const std::size_t vdata_offs = sect_header.virtual_address;
		void * const outmem_ptr = into_memory.data() + vdata_offs;
		memory_manager.commit(outmem_ptr, sect_header.virtual_size);
		into_memory.write(vdata_offs, rdata_buf.data(), rdata_buf.size());
	}
}

template <class PEFileImage, class MemoryBlock>
void map_pe_image_sections_direct(const PEFileImage & image, MemoryBlock & into_memory)
{
	assert(into_memory.memory_manager().allows_direct_addressing());

	MemoryManager & memory_manager = into_memory.memory_manager();
	for (const auto & sect_header : image.section_headers()) {
		const std::size_t vdata_offs = sect_header.virtual_address;
		const peplus::FileOffset rdata_offs { sect_header.pointer_to_raw_data };

		char * const outmem_ptr = into_memory.data() + vdata_offs;
		const std::size_t rem_size = into_memory.size() - vdata_offs;
		if (rem_size < sect_header.size_of_raw_data)
			throw std::runtime_error("Invalid section header");

		memory_manager.commit(outmem_ptr, sect_header.virtual_size);
		image.read(rdata_offs, sect_header.size_of_raw_data, outmem_ptr);
	}
}

template <class PEVirtualImage, class MemoryBlock>
void apply_pe_relocation_entry(const PEVirtualImage          & image,
                               const peplus::BaseRelocation  & base_reloc,
                               const peplus::RelocationEntry & reloc_entry,
                               std::uintptr_t                  base_address,
                               MemoryBlock                   & relblock_mem)
{
	const auto reloc_offs = reloc_entry.address - base_reloc.virtual_address;
	const std::uintptr_t preferred_base = image.optional_header().image_base;
	const std::ptrdiff_t base_diff = base_address - preferred_base;

	switch (reloc_entry.type) {
		case peplus::REL_BASED_ABSOLUTE:
			break;

		case peplus::REL_BASED_HIGHLOW:
			modify_le_value_at<std::uint32_t>(relblock_mem, reloc_offs.value(),
				[=] (auto x) { return x + std::int32_t(base_diff); });
			break;

		case peplus::REL_BASED_DIR64:
			modify_le_value_at<std::uint64_t>(relblock_mem, reloc_offs.value(),
				[=] (auto x) { return x + std::int64_t(base_diff); });
			break;

		default:
			throw std::runtime_error("Unsupported relocation type");
	}
}

template <class PEImage, class MemoryBlock>
void apply_pe_image_relocations_direct(const PEImage & image, MemoryBlock & image_mem)
{
	assert(image_mem.memory_manager().allows_direct_addressing());

	const auto base_address = reinterpret_cast<std::uintptr_t>(image_mem.data());
	for (const auto & base_reloc : image.base_relocations()) {
		const std::size_t relblock_size = base_reloc.size_of_block;
		auto relblock_slice = get_memory_block_slice(image_mem, base_reloc.virtual_address, relblock_size);
		for (const auto & reloc_entry : base_reloc.entries())
			apply_pe_relocation_entry(image, base_reloc, reloc_entry, base_address, relblock_slice);
	}
}

template <class PEImage, class MemoryBlock>
void apply_pe_image_relocations_indirect(const PEImage & image, MemoryBlock & image_mem)
{
	const auto base_address = reinterpret_cast<std::uintptr_t>(image_mem.data());
	for (const auto & base_reloc : image.base_relocations()) {
		const std::size_t relblock_size = base_reloc.size_of_block;
		auto relblock_slice = get_memory_block_slice(image_mem, base_reloc.virtual_address, relblock_size);
		OwnedMemoryBlock relblock_copy = copy_into_local_memory(relblock_slice);
		for (const auto & reloc_entry : base_reloc.entries())
			apply_pe_relocation_entry(image, base_reloc, reloc_entry, base_address, relblock_copy);
		relblock_slice.write(0, relblock_copy.data(), relblock_size);
	}
}

template <class PEImportDescriptor, class MemoryBlock>
void resolve_pe_imported_symbols(const PEImportDescriptor & import_dtor,
                                 const Module & module, MemoryBlock & image_mem)
{
	auto thunks_it = import_dtor.thunks().begin();
	for (const auto & import_entry : import_dtor.entries()) {
		std::visit([&] (const auto & import_info) {
			if constexpr (import_dtor.template is_unnamed_import<decltype(import_info)>()) {
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

template <class PEImage, class MemoryBlock>
void resolve_pe_image_imports(const PEImage  & image,
                              ModuleProvider & mod_provider,
                              MemoryBlock    & image_mem)
{
	for (const auto & import_dtor : image.import_descriptors()) {
		const std::string mod_name = import_dtor.name_str();
		const auto mod_sp = mod_provider.get_module(mod_name);
		if (!mod_sp)
			throw std::runtime_error("Image has unresolved imports");
		resolve_pe_imported_symbols(import_dtor, *mod_sp, image_mem);
	}
}

constexpr int pe_section_memory_access(int characteristics)
{
	int mem_access = 0;
	if (characteristics & peplus::SCN_MEM_READ)    mem_access |= MemoryManager::ReadAccess;
	if (characteristics & peplus::SCN_MEM_WRITE)   mem_access |= MemoryManager::WriteAccess;
	if (characteristics & peplus::SCN_MEM_EXECUTE) mem_access |= MemoryManager::ExecuteAccess;
	return mem_access;
}

template <class PEImage, class MemoryBlock>
void apply_pe_memory_permissions(const PEImage & image, MemoryBlock & image_mem)
{
	MemoryManager & memory_manager = image_mem.memory_manager();
	for (const auto & sect_header : image.section_headers()) {
		const std::size_t vdata_offs = sect_header.virtual_address;
		void * const mem_ptr = image_mem.data() + vdata_offs;

		const int mem_access = pe_section_memory_access(sect_header.characteristics);
		memory_manager.set_access(mem_ptr, sect_header.virtual_size, mem_access);
	}
}

template <class PEImage, class MemoryBlock>
void remove_pe_discardable_sections(const PEImage & image, MemoryBlock & image_mem)
{
	MemoryManager & memory_manager = image_mem.memory_manager();
	for (const auto & section_header : image.section_headers()) {
		if (section_header.characteristics & peplus::SCN_MEM_DISCARDABLE) {
			const std::size_t vdata_size = section_header.virtual_size;
			const std::size_t vdata_offs = section_header.virtual_address;
			void * const vdata_ptr = image_mem.data() + vdata_offs;
			memory_manager.decommit(vdata_ptr, vdata_size);
		}
	}
}

template <unsigned int XX>
OwnedMemoryBlock load_pe_image(const MemoryBuffer & image_data,
                               MemoryManager      & memory_manager,
                               ModuleProvider     & mod_provider)
{
	const peplus::FileImage<XX, any_buffer> src_image { image_data };
	auto image_mem = detail::allocate_pe_image(src_image, memory_manager);
	if (memory_manager.allows_direct_addressing()) {
		detail::copy_pe_image_headers_direct(src_image, image_mem);
		detail::map_pe_image_sections_direct(src_image, image_mem);
	} else {
		detail::copy_pe_image_headers_indirect(src_image, image_mem);
		detail::map_pe_image_sections_indirect(src_image, image_mem);
	}

	const peplus::VirtualImage<XX, any_buffer> dst_image { image_mem };
	if (memory_manager.allows_direct_addressing()) {
		detail::apply_pe_image_relocations_direct(dst_image, image_mem);
	} else {
		detail::apply_pe_image_relocations_indirect(dst_image, image_mem);
	}
	detail::resolve_pe_image_imports(dst_image, mod_provider, image_mem);
	detail::apply_pe_memory_permissions(dst_image, image_mem);
	detail::remove_pe_discardable_sections(dst_image, image_mem);
	return image_mem;
}

template <class PEImage>
inline std::size_t pe_image_exception_table_size(const PEImage & image)
{
	const auto except_dir = image.data_directory(peplus::DIRECTORY_ENTRY_EXCEPTION);
	return except_dir ? except_dir->size : 0;
}

template <class PEImage>
void register_pe_image_exception_table(const PEImage & image,
                                       Process       & process,
                                       void          * image_mem)
{
	if (const auto rftable = image.exception_entries()) {
		const auto rftable_offset = rftable.begin()->offset();
		const auto image_base = reinterpret_cast<std::uintptr_t>(image_mem);
		const std::size_t rftable_size = pe_image_exception_table_size(image);
		const auto rftable_ptr = static_cast<char *>(image_mem) + rftable_offset.value();
		process.register_exception_table(image_base, rftable_ptr, rftable_size);
	}
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
void * pe_image_entry_point(const PEVirtualImage & image, void * image_base)
{
	const auto opt_header = image.optional_header();
	char * const image_ptr = static_cast<char *>(image_base);
	return image_ptr + opt_header.address_of_entry_point;
}

template <class PEVirtualImage>
int invoke_dll_entry_point_direct(const PEVirtualImage & image,
                                  void * image_base, DWORD event,
                                  void * reserved)
{
	assert(image.type() == peplus::ImageType::Dynamic);

	const void * entry_point = pe_image_entry_point(image, image_base);
	const auto dll_main = reinterpret_cast<DllMain>(entry_point);
	return dll_main(image_base, event, reserved);
}

template <class PEImage>
int notify_dll_event_direct(const PEImage & image, void * image_base,
                            DWORD event, void * reserved = nullptr)
{
	invoke_pe_tls_callbacks_direct(image, image_base, event, reserved);
	return invoke_dll_entry_point_direct(image, image_base, event, reserved);
}

template <class PEVirtualImage>
std::unique_ptr<CodeChunk> notify_dll_event_indirect(const PEVirtualImage & image,
                                                     void                 * image_base,
                                                     CodeGenerator        & code_generator,
                                                     DWORD                  event,
                                                     void                 * reserved = nullptr)
{
	const AbsoluteCodeLocation dll_main = pe_image_entry_point(image, image_base);
	const CallingConvention & stdcall_cconv = code_generator.get_calling_convention("stdcall");
	return stdcall_cconv.invoke_proc(dll_main, make_proc_params(image_base, event, reserved));
}

template <class PEImage>
bool initialize_dll_direct(const PEImage & image, Process & process, void * image_base)
{
	assert(process.memory_manager().allows_direct_addressing());

	return notify_dll_event_direct(image, image_base, DLL_PROCESS_ATTACH)
	    && notify_dll_event_direct(image, image_base, DLL_THREAD_ATTACH);
}

template <class PEImage, class MemoryBlock>
bool initialize_dll(const PEImage & image, Process & process, MemoryBlock & image_mem)
{
	register_pe_image_exception_table(image, process, image_mem.data());

	if (process.memory_manager().allows_direct_addressing()) {
		return initialize_dll_direct(image, process, image_mem.data());
	} else {
		// initialize_dll_indirect(image, process, image_mem);
	}
}

template <class PEImage>
void deinitialize_dll_direct(const PEImage & image, Process & process, void * image_base)
{
	assert(process.memory_manager().allows_direct_addressing());

	notify_dll_event_direct(image, image_base, DLL_THREAD_DETACH);
	notify_dll_event_direct(image, image_base, DLL_PROCESS_DETACH);
}

template <class PEImage>
void deregister_pe_image_exception_table(const PEImage & image,
                                         Process       & process,
                                         void          * image_mem)
{
	if (const auto rftable = image.exception_entries()) {
		const auto rftable_offset = rftable.begin()->offset();
		const auto rftable_ptr = static_cast<char *>(image_mem) + rftable_offset.value();
		process.deregister_exception_table(rftable_ptr);
	}
}

template <class PEImage, class MemoryBlock>
void deinitialize_dll(const PEImage & image, Process & process, MemoryBlock & image_mem)
{
	if (process.memory_manager().allows_direct_addressing()) {
		deinitialize_dll_direct(image, process, image_mem.data());
	} else {
		// deinitialize_dll_indirect(image, process, image_mem);
	}

	deregister_pe_image_exception_table(image, process, image_mem.data());
}

}

#endif
