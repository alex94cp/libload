#ifndef LOAD_MEMORY_HPP_
#define LOAD_MEMORY_HPP_

#include <load/export.hpp>

#include <cstddef>
#include <cstdint>
#include <future>

namespace load {

enum MemoryAccess
{
	MemAccessNone    = 0,
	MemAccessRead    = 1 << 0,
	MemAccessWrite   = 1 << 1,
	MemAccessExecute = 1 << 2,
};

class LOAD_EXPORT MemoryBuffer
{
public:
	virtual ~MemoryBuffer() = default;
	virtual std::size_t read(std::size_t offset, std::size_t size, void * into_buffer) const = 0;
};

class LOAD_EXPORT MemoryManager
{
public:
	virtual ~MemoryManager() = default;

	virtual bool allows_direct_addressing() const = 0;

	virtual void * allocate(std::uintptr_t base, std::size_t size) = 0;
	virtual void release(void * mem, std::size_t size) = 0;

	virtual void commit(void * mem, std::size_t size) = 0;
	virtual void decommit(void * mem, std::size_t size) = 0;
	virtual void set_access(void * mem, std::size_t size, int access) = 0;

	virtual std::size_t copy_from(const void * mem, std::size_t size, void * into_buffer) = 0;
	virtual std::size_t copy_into(const void * from_buffer, std::size_t size, void * into_mem) = 0;

	virtual std::future<int> run_async(const void * mem, void * params) = 0;
};

LOAD_EXPORT extern MemoryManager & local_memory_manager;

}

#endif