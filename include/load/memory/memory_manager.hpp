#ifndef LOAD_MEMORY_MEMORYMANAGER_HPP_
#define LOAD_MEMORY_MEMORYMANAGER_HPP_

#include <load/export.hpp>

#include <cstddef>
#include <cstdint>

namespace load {

class LOAD_EXPORT MemoryManager
{
public:
	enum MemoryAccess
	{
		ReadAccess    = 1 << 0,
		WriteAccess   = 1 << 1,
		ExecuteAccess = 1 << 2,
	};

	virtual ~MemoryManager() = default;

	virtual bool allows_direct_addressing() const = 0;

	virtual void * allocate(std::uintptr_t base, std::size_t size) = 0;
	virtual void release(void * mem, std::size_t size) = 0;

	virtual void commit(void * mem, std::size_t size) = 0;
	virtual void decommit(void * mem, std::size_t size) = 0;
	virtual void set_access(void * mem, std::size_t size, int access) = 0;

	virtual std::size_t copy_from(const void * mem, std::size_t size, void * into_buffer) = 0;
	virtual std::size_t copy_into(const void * from_buffer, std::size_t size, void * into_mem) = 0;
};

}

#endif