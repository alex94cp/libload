#ifndef LOAD_SRC_MEMORYBLOCK_HPP_
#define LOAD_SRC_MEMORYBLOCK_HPP_

#include <load/memory/memory_buffer.hpp>
#include <load/memory/memory_manager.hpp>

#include <boost/endian/conversion.hpp>

#include <algorithm>
#include <functional>
#include <stdexcept>
#include <type_traits>

namespace load::detail {

struct any_buffer
{
	using value_type = const MemoryBuffer &;

	static std::size_t read(const MemoryBuffer & from_buffer,
	                        std::size_t offset, std::size_t size,
	                        void * into_buffer)
	{
		return from_buffer.read(offset, size, into_buffer);
	}
};

struct owned_memory
{
	static void dispose(MemoryManager & mem_manager,
	                    void * mem, std::size_t size)
	{
		mem_manager.release(mem, size);
	}
};

struct borrowed_memory
{
	static void dispose(MemoryManager &, void *, std::size_t) {}
};

class MutableMemoryBuffer : public MemoryBuffer
{
public:
	virtual std::size_t write(std::size_t  offset,
	                          const void * from_buffer,
	                          std::size_t  size) = 0;
};

template <class MemoryOwnership>
class MemoryBlock final : public MutableMemoryBuffer
{
public:
	friend class MemoryBlock;

	MemoryBlock(MemoryManager & mem_manager,
	            void * mem, std::size_t size);
	
	template <class MO2>
	MemoryBlock(MemoryBlock<MO2> && other);
	
	virtual ~MemoryBlock();

	char       * data();
	const char * data() const;
	std::size_t  size() const;

	explicit operator bool() const;

	MemoryManager       & memory_manager();
	const MemoryManager & memory_manager() const;

	void release();

	virtual std::size_t read(std::size_t offset,
	                         std::size_t size,
	                         void      * into_buffer) const override;
	
	virtual std::size_t write(std::size_t  offset,
	                          const void * from_buffer,
	                          std::size_t  size) override;

private:
	MemoryManager * _mem_manager;
	char          * _mem_pointer;
	std::size_t     _mem_size;
};

using OwnedMemoryBlock    = MemoryBlock<owned_memory>;
using BorrowedMemoryBlock = MemoryBlock<borrowed_memory>;

template <class MemoryBlock>
BorrowedMemoryBlock get_memory_block_slice(MemoryBlock & mem_block,
                                           std::size_t   offset,
                                           std::size_t   size)
{
	if (offset + size >= mem_block.size())
		throw std::out_of_range("Memory range not withing block");
	
	MemoryManager & mem_manager = mem_block.memory_manager();
	const std::size_t slice_size = std::min(size, mem_block.size() - offset);
	return BorrowedMemoryBlock(mem_manager, mem_block.data(), slice_size);
}

template <class MemoryBlock>
OwnedMemoryBlock copy_into_local_memory(const MemoryBlock & mem_block)
{
	const std::size_t mem_size = mem_block.size();
	MemoryManager & mem_manager = current_process().memory_manager();
	OwnedMemoryBlock mem_copy { mem_manager, mem_manager.allocate(0, mem_size), mem_size };
	mem_block.read(0, mem_size, mem_copy.data());
	return mem_copy;
}

template <typename T, typename = std::enable_if_t<std::is_trivial_v<T>>>
T read_le_value_from(const MemoryBuffer & mem_buffer, std::size_t offset)
{
	T value;
	mem_buffer.read(offset, sizeof(T), &value);
	return boost::endian::little_to_native(value);
}

template <typename T, typename = std::enable_if_t<std::is_trivial_v<T>>>
void write_le_value_into(T value, MutableMemoryBuffer & mem_buffer, std::size_t offset)
{
	boost::endian::native_to_little_inplace(value);
	mem_buffer.write(offset, &value, sizeof(T));
}

template <typename T, typename Fn, typename = std::enable_if_t<std::is_trivial_v<T>>>
void modify_le_value_at(MutableMemoryBuffer & mem_buffer, std::size_t offset, Fn && fn)
{
	T value = read_le_value_from<T>(mem_buffer, offset);
	value = std::invoke<Fn>(std::forward<Fn>(fn), value);
	write_le_value_into<T>(value, mem_buffer, offset);
}

template <class MO>
MemoryBlock<MO>::MemoryBlock(MemoryManager & mem_manager,
                             void * mem, std::size_t size)
	: _mem_manager { &mem_manager }
	, _mem_pointer { static_cast<char *>(mem) }
	, _mem_size { size }
{}

template <class MO> template <class MO2>
MemoryBlock<MO>::MemoryBlock(MemoryBlock<MO2> && other)
	: _mem_manager { other._mem_manager }
	, _mem_pointer { other._mem_pointer }
	, _mem_size { other._mem_size }
{
	other.release();
}

template <class MO>
MemoryBlock<MO>::~MemoryBlock()
{
	if (_mem_pointer != nullptr)
		MO::dispose(*_mem_manager, _mem_pointer, _mem_size);
}

template <class MO>
char * MemoryBlock<MO>::data()
{
	return _mem_pointer;
}

template <class MO>
const char * MemoryBlock<MO>::data() const
{
	return _mem_pointer;
}

template <class MO>
std::size_t MemoryBlock<MO>::size() const
{
	return _mem_size;
}

template <class MO>
MemoryBlock<MO>::operator bool() const
{
	return _mem_pointer != nullptr;
}

template <class MO>
MemoryManager & MemoryBlock<MO>::memory_manager()
{
	return *_mem_manager;
}

template <class MO>
const MemoryManager & MemoryBlock<MO>::memory_manager() const
{
	return *_mem_manager;
}

template <class MO>
void MemoryBlock<MO>::release()
{
	_mem_manager = nullptr;
	_mem_pointer = nullptr;
	_mem_size = 0;
}

template <class MO>
std::size_t MemoryBlock<MO>::read(std::size_t offset,
                                  std::size_t size,
                                  void      * into_buffer) const
{
	const std::size_t bytes_to_read = std::min(_mem_size - offset, size);
	return _mem_manager->copy_from(_mem_pointer + offset, bytes_to_read, into_buffer);
}

template <class MO>
std::size_t MemoryBlock<MO>::write(std::size_t  offset,
                                   const void * from_buffer,
                                   std::size_t  size)
{
	const std::size_t bytes_to_write = std::min(_mem_size - offset, size);
	return _mem_manager->copy_into(from_buffer, bytes_to_write, _mem_pointer + offset);
}

}

#endif