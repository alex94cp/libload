#include "code_chunk.hpp"

#include <numeric>
#include <utility>

namespace load::detail {

AbsoluteCodeLocation::AbsoluteCodeLocation(void * address)
	: _address { static_cast<char *>(address) } {}

std::unique_ptr<CodeLocation> AbsoluteCodeLocation::clone() const
{
	return std::make_unique<AbsoluteCodeLocation>(*this);
}

std::ptrdiff_t AbsoluteCodeLocation::distance_from(void * location) const
{
	return static_cast<char *>(location) - _address;
}

std::uintptr_t AbsoluteCodeLocation::absolute_address(void * location) const
{
	return reinterpret_cast<std::uintptr_t>(_address);
}

CodeChunkLiteral::CodeChunkLiteral(std::string_view data)
	: _data { data } {}

std::size_t CodeChunkLiteral::max_size() const
{
	return _data.size();
}

std::size_t CodeChunkLiteral::emit(void * location, CodeSink & code_sink) const
{
	code_sink.append(_data.data(), _data.size());
	return _data.size();
}

void FixedCodeChunk::append(char byte)
{
	_data.push_back(byte);
}

void FixedCodeChunk::append(std::string_view data)
{
	std::copy(data.begin(), data.end(), std::back_inserter(_data));
}

std::size_t FixedCodeChunk::max_size() const
{
	return _data.size();
}

std::size_t FixedCodeChunk::emit(void * location, CodeSink & code_sink) const
{
	code_sink.append(_data.data(), _data.size());
	return _data.size();
}

void CodeBlock::add(std::unique_ptr<CodeChunk> code_chunk)
{
	_code_chunks.emplace_back(std::move(code_chunk));
}

std::size_t CodeBlock::max_size() const
{
	return std::accumulate(_code_chunks.begin(), _code_chunks.end(), std::size_t(0),
		[] (std::size_t block_size, auto & code_chunk)
	{
		return block_size + code_chunk->max_size();
	});
}

std::size_t CodeBlock::emit(void * location, CodeSink & code_sink) const
{
	return std::accumulate(_code_chunks.begin(), _code_chunks.end(), std::size_t(0),
		[&] (std::size_t bytes_emitted, auto & code_chunk)
	{
		const auto bytes_ptr = static_cast<char *>(location) + bytes_emitted;
		return bytes_emitted + code_chunk->emit(bytes_ptr, code_sink);
	});
}

MemoryBufferCodeSink::MemoryBufferCodeSink(MutableMemoryBuffer & buffer)
	: _buffer { &buffer }, _offset { 0 } {}

void MemoryBufferCodeSink::append(const char * data, std::size_t size)
{
	_buffer->write(_offset, data, size);
	_offset += size;
}

}