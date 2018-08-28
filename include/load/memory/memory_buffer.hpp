#ifndef LOAD_MEMORY_MEMORYBUFFER_HPP_
#define LOAD_MEMORY_MEMORYBUFFER_HPP_

#include <load/export.hpp>

#include <cstddef>

namespace load {

class LOAD_EXPORT MemoryBuffer
{
public:
	virtual ~MemoryBuffer() = default;
	virtual std::size_t read(std::size_t offset, std::size_t size, void * into_buffer) const = 0;
};

}

#endif