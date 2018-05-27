#ifndef LOAD_SRC_MEMORY_HPP_
#define LOAD_SRC_MEMORY_HPP_

#include <load/memory.hpp>

#include <boost/endian/conversion.hpp>

#include <functional>
#include <type_traits>

namespace load::detail {

class MemoryBlock final : public MemoryBuffer
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

	virtual std::size_t read(std::size_t offset, std::size_t size,
	                         void * into_buffer) const override;

	std::size_t write(std::size_t offset, std::size_t size, const void * from_buffer);

private:
	MemoryManager * _mem_manager;
	char          * _mem_pointer;
	std::size_t     _mem_size;
};

template <typename T, typename = std::enable_if_t<std::is_trivial_v<T>>>
T read_le_value_from(const MemoryBlock & mem_block, std::size_t offset)
{
	T value;
	mem_block.read(offset, sizeof(T), &value);
	return boost::endian::little_to_native(value);
}

template <typename T, typename = std::enable_if_t<std::is_trivial_v<T>>>
void write_le_value_into(T value, MemoryBlock & mem_block, std::size_t offset)
{
	boost::endian::native_to_little_inplace(value);
	mem_block.write(offset, sizeof(T), &value);
}

template <typename T, typename Fn, typename = std::enable_if_t<std::is_trivial_v<T>>>
void modify_le_value_at(MemoryBlock & mem_block, std::size_t offset, Fn && fn)
{
	T value = read_le_value_from<T>(mem_block, offset);
	value = std::invoke<Fn>(std::forward<Fn>(fn), value);
	write_le_value_into<T>(value, mem_block, offset);
}

}

#endif