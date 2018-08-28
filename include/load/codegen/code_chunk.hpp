#ifndef LOAD_CODEGEN_CODECHUNK_HPP_
#define LOAD_CODEGEN_CODECHUNK_HPP_

#include <load/export.hpp>

#include <cstddef>

namespace load {

class LOAD_EXPORT CodeSink
{
public:
	virtual ~CodeSink() = default;
	virtual void append(const char * data, std::size_t size) = 0;
};

class LOAD_EXPORT CodeChunk
{
public:
	virtual ~CodeChunk() = default;
	virtual std::size_t max_size() const = 0;
	virtual std::size_t emit(void * location, CodeSink & code_sink) const = 0;
};

}

#endif