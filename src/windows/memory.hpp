#ifndef LOAD_SRC_WINDOWS_MEMORY_HPP_
#define LOAD_SRC_WINDOWS_MEMORY_HPP_

#include "../memory.hpp"

#include <windows.h>

namespace load::detail {

class CurrentProcessMemory final : public MemoryManager
{
public:
	virtual bool allows_direct_addressing() const override;

	virtual void * allocate(std::uintptr_t base, std::size_t size) override;
	virtual void set_access(void * mem, std::size_t size, int access) override;
	virtual void release(void * mem, std::size_t size) override;

	virtual std::size_t copy_from(const void * mem, std::size_t size, void * into_buffer) override;
	virtual std::size_t copy_into(const void * data, std::size_t size, void * into_mem) override;

	virtual void commit(void * mem, std::size_t size) override;
	virtual void decommit(void * mem, std::size_t size) override;
};

class LocalProcessMemory final : public MemoryManager
{
public:
	explicit LocalProcessMemory(HANDLE handle);

	virtual bool allows_direct_addressing() const override;

	virtual void * allocate(std::uintptr_t base, std::size_t size) override;
	virtual void set_access(void * mem, std::size_t size, int access) override;
	virtual void release(void * mem, std::size_t size) override;

	virtual std::size_t copy_from(const void * mem, std::size_t size, void * into_buffer) override;
	virtual std::size_t copy_into(const void * data, std::size_t size, void * into_mem) override;

	virtual void commit(void * mem, std::size_t size) override;
	virtual void decommit(void * mem, std::size_t size) override;

private:
	HANDLE _handle;
};

}

#endif