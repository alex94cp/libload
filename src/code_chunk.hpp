#ifndef LOAD_SRC_CODECHUNK_HPP_
#define LOAD_SRC_CODECHUNK_HPP_

#include "memory_block.hpp"

#include <load/codegen/common.hpp>
#include <load/codegen/code_chunk.hpp>
#include <load/codegen/code_generator.hpp>
#include <load/codegen/calling_convention.hpp>

#include <memory>
#include <string_view>
#include <vector>

namespace load::detail {

class AbsoluteCodeLocation final : public CodeLocation
{
public:
	explicit AbsoluteCodeLocation(void * address);

	virtual std::unique_ptr<CodeLocation> clone() const override;
	virtual std::ptrdiff_t distance_from(void * location) const override;
	virtual std::uintptr_t absolute_address(void * location) const override;

private:
	char * _address;
};

class CodeChunkLiteral final : public CodeChunk
{
public:
	explicit CodeChunkLiteral(std::string_view data);

	virtual std::size_t max_size() const override;
	virtual std::size_t emit(void * location, CodeSink & code_sink) const override;

private:
	std::string_view _data;
};

class FixedCodeChunk final : public CodeChunk
{
public:
	void append(char byte);
	void append(std::string_view data);

	virtual std::size_t max_size() const override;
	virtual std::size_t emit(void * location, CodeSink & code_sink) const override;

private:
	std::vector<char> _data;
};

class CodeBlock final : public CodeChunk
{
public:
	void add(std::unique_ptr<CodeChunk> code_chunk);

	virtual std::size_t max_size() const override;
	virtual std::size_t emit(void * location, CodeSink & code_sink) const override;

private:
	std::vector<std::unique_ptr<CodeChunk>> _code_chunks;
};

class MemoryBufferCodeSink final : public CodeSink
{
public:
	explicit MemoryBufferCodeSink(MutableMemoryBuffer & buffer);

	virtual void append(const char * data, std::size_t size) override;

private:
	MutableMemoryBuffer * _buffer;
	std::size_t _offset;
};

template <typename... Params>
ParameterList make_proc_params(Params... params)
{
	static_assert((std::is_integral_v<Params> && ...));
	return { std::uintmax_t(params)... };
}

}

#endif