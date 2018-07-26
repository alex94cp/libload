#ifndef LOAD_SRC_MEMORY_HPP_
#define LOAD_SRC_MEMORY_HPP_

#include <load/memory.hpp>

#include <boost/endian/conversion.hpp>

#include <algorithm>
#include <functional>
#include <tuple>
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

template <class MemoryOwnershipPolicy>
class MemoryBlock final : public MutableMemoryBuffer
{
public:
	MemoryBlock(MemoryManager & mem_manager,
	            void * mem, std::size_t size);
	MemoryBlock(MemoryBlock && other);
	virtual ~MemoryBlock();

	char       * data();
	const char * data() const;
	std::size_t  size() const;

	MemoryManager       & memory_manager();
	const MemoryManager & memory_manager() const;

	virtual std::size_t read(std::size_t offset,
	                         std::size_t size,
	                         void      * into_buffer) const override;
	
	virtual std::size_t write(std::size_t  offset,
	                          const void * from_buffer,
	                          std::size_t  size) override;

	std::tuple<MemoryManager *, void *, std::size_t> release();

private:
	MemoryManager * _mem_manager;
	char          * _mem_pointer;
	std::size_t     _mem_size;
};

using OwnedMemoryBlock    = MemoryBlock<owned_memory>;
using BorrowedMemoryBlock = MemoryBlock<borrowed_memory>;

template <typename T, class ReadableBuffer, typename = std::enable_if_t<std::is_trivial_v<T>>>
T read_le_value_from(const ReadableBuffer & mem_buffer, std::size_t offset)
{
	T value;
	mem_buffer.read(offset, sizeof(T), &value);
	return boost::endian::little_to_native(value);
}

template <typename T, class WritableBuffer, typename = std::enable_if_t<std::is_trivial_v<T>>>
void write_le_value_into(T value, WritableBuffer & mem_buffer, std::size_t offset)
{
	boost::endian::native_to_little_inplace(value);
	mem_buffer.write(offset, &value, sizeof(T));
}

template <typename T, class RWBuffer, typename Fn, typename = std::enable_if_t<std::is_trivial_v<T>>>
void modify_le_value_at(RWBuffer & mem_buffer, std::size_t offset, Fn && fn)
{
	T value = read_le_value_from<T>(mem_buffer, offset);
	value = std::invoke<Fn>(std::forward<Fn>(fn), value);
	write_le_value_into<T>(value, mem_buffer, offset);
}

template <class MOP>
MemoryBlock<MOP>::MemoryBlock(MemoryManager & mem_manager,
                              void * mem, std::size_t size)
	: _mem_manager { &mem_manager }
	, _mem_pointer { static_cast<char *>(mem) }
	, _mem_size { size }
{}

template <class MOP>
MemoryBlock<MOP>::MemoryBlock(MemoryBlock && other)
	: _mem_manager { other._mem_manager }
	, _mem_pointer { other._mem_pointer }
	, _mem_size { other._mem_size }
{
	other._mem_pointer = nullptr;
}

template <class MOP>
MemoryBlock<MOP>::~MemoryBlock()
{
	if (_mem_pointer != nullptr)
		MOP::dispose(*_mem_manager, _mem_pointer, _mem_size);
}

template <class MOP>
char * MemoryBlock<MOP>::data()
{
	return _mem_pointer;
}

template <class MOP>
const char * MemoryBlock<MOP>::data() const
{
	return _mem_pointer;
}

template <class OP>
std::size_t MemoryBlock<OP>::size() const
{
	return _mem_size;
}

template <class MOP>
MemoryManager & MemoryBlock<MOP>::memory_manager()
{
	return *_mem_manager;
}

template <class MOP>
const MemoryManager & MemoryBlock<MOP>::memory_manager() const
{
	return *_mem_manager;
}

template <class MOP>
std::size_t MemoryBlock<MOP>::read(std::size_t offset,
                                   std::size_t size,
                                   void      * into_buffer) const
{
	const std::size_t bytes_to_read = std::min(_mem_size - offset, size);
	return _mem_manager->copy_from(_mem_pointer + offset, bytes_to_read, into_buffer);
}

template <class MOP>
std::size_t MemoryBlock<MOP>::write(std::size_t  offset,
                                    const void * from_buffer,
                                    std::size_t  size)
{
	const std::size_t bytes_to_write = std::min(_mem_size - offset, size);
	return _mem_manager->copy_into(from_buffer, bytes_to_write, _mem_pointer + offset);
}

template <class MOP>
std::tuple<MemoryManager *, void *, std::size_t> MemoryBlock<MOP>::release()
{
	std::tuple data { _mem_manager, _mem_pointer, _mem_size };
	_mem_manager = nullptr;
	_mem_pointer = nullptr;
	_mem_size = 0;
	return data;
}

}

#endif