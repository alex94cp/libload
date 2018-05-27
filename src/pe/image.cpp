#include "image.hpp"

namespace load::detail {

PEPlusMemoryBufferAdapter::PEPlusMemoryBufferAdapter(const load::MemoryBuffer & mem_buffer)
	: _mem_buffer { &mem_buffer } {}

std::size_t PEPlusMemoryBufferAdapter::read(std::size_t offset, std::size_t size, void * into_buffer) const
{
	return _mem_buffer->read(offset, size, into_buffer);
}

}