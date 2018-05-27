#include <config.hpp>
#include "memory.hpp"

#include <load/module.hpp>

#if defined(LIBLOAD_ENABLE_PE64_SUPPORT) || defined(LIBLOAD_ENABLE_PE32_SUPPORT)
#	include "pe/module.hpp"
#endif

#include <algorithm>

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

namespace detail {

MemoryBlock::MemoryBlock(MemoryManager & mem_manager,
                         void * mem, std::size_t size)
	: _mem_manager { &mem_manager }
	, _mem_pointer { static_cast<char *>(mem) }
	, _mem_size    { size }
{}

MemoryBlock::MemoryBlock(MemoryBlock && other)
	: _mem_manager { other._mem_manager }
	, _mem_pointer { other._mem_pointer }
	, _mem_size    { other._mem_size    }
{
	other._mem_pointer = nullptr;
}

MemoryBlock::~MemoryBlock()
{
	if (_mem_pointer != nullptr)
		_mem_manager->release(_mem_pointer, _mem_size);
}

char * MemoryBlock::data()
{
	return _mem_pointer;
}

const char * MemoryBlock::data() const
{
	return _mem_pointer;
}

std::size_t MemoryBlock::size() const
{
	return _mem_size;
}

MemoryManager & MemoryBlock::memory_manager()
{
	return *_mem_manager;
}

const MemoryManager & MemoryBlock::memory_manager() const
{
	return *_mem_manager;
}

std::size_t MemoryBlock::read(std::size_t offset, std::size_t size, void * into_buffer) const
{
	const std::size_t bytes_to_read = std::min(_mem_size - offset, size);
	return _mem_manager->copy_from(_mem_pointer + offset, bytes_to_read, into_buffer);
}

std::size_t MemoryBlock::write(std::size_t offset, std::size_t size, const void * from_buffer)
{
	const std::size_t bytes_to_write = std::min(_mem_size - offset, size);
	return _mem_manager->copy_into(from_buffer, bytes_to_write, _mem_pointer + offset);
}

} }