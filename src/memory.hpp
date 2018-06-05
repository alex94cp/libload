#ifndef LOAD_SRC_MEMORY_HPP_
#define LOAD_SRC_MEMORY_HPP_

#include <load/memory.hpp>

#include <boost/endian/conversion.hpp>

#include <functional>
#include <type_traits>

namespace load::detail {

struct any_buffer
{
	using value_type = const MemoryBuffer &;

	static std::size_t read(const MemoryBuffer & from_buffer, std::size_t offset,
	                        std::size_t size, void * into_buffer)
	{
		return from_buffer.read(offset, size, into_buffer);
	}
};

class LOAD_EXPORT MutableMemoryBuffer : public MemoryBuffer
{
public:
	virtual ~MutableMemoryBuffer() = default;
	virtual std::size_t write(std::size_t offset, const void * from_buffer, std::size_t size) = 0;
};

class MemoryBlock final : public MutableMemoryBuffer
{
public:
	MemoryBlock(MemoryManager & mem_manager, void * mem, std::size_t size);
	MemoryBlock(MemoryBlock && other);
	~MemoryBlock();

	char * data();
	const char * data() const;

	std::size_t size() const;

	MemoryManager & memory_manager();
	const MemoryManager & memory_manager() const;

	virtual std::size_t read(std::size_t offset, std::size_t size, void * into_buffer) const override;
	virtual std::size_t write(std::size_t offset, const void * from_buffer, std::size_t size) override;

private:
	MemoryManager * _mem_manager;
	char          * _mem_pointer;
	std::size_t     _mem_size;
};

template <typename T, class ReadBuffer, typename = std::enable_if_t<std::is_trivial_v<T>>>
T read_le_value_from(const ReadBuffer & mem_buffer, std::size_t offset)
{
	T value;
	mem_buffer.read(offset, sizeof(T), &value);
	return boost::endian::little_to_native(value);
}

template <typename T, class WriteBuffer, typename = std::enable_if_t<std::is_trivial_v<T>>>
void write_le_value_into(T value, WriteBuffer & mem_buffer, std::size_t offset)
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

}

#endif