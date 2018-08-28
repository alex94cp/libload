#ifndef LOAD_CODEGEN_CALLINGCONVENTION_HPP_
#define LOAD_CODEGEN_CALLINGCONVENTION_HPP_

#include <load/export.hpp>
#include <load/codegen/common.hpp>

#include <cstdint>
#include <memory>
#include <vector>

namespace load {

class CodeChunk;

class LOAD_EXPORT CodeLocation
{
public:
	virtual ~CodeLocation() = default;
	virtual std::unique_ptr<CodeLocation> clone() const = 0;
	virtual std::ptrdiff_t distance_from(void * location) const = 0;
	virtual std::uintptr_t absolute_address(void * location) const = 0;
};

class LOAD_EXPORT CallingConvention
{
public:
	virtual ~CallingConvention() = default;

	virtual std::unique_ptr<CodeChunk> make_prolog(unsigned int argc) const = 0;
	virtual std::unique_ptr<CodeChunk> make_epilog(unsigned int argc) const = 0;
	virtual std::unique_ptr<CodeChunk> set_return_value(std::uintmax_t value) const = 0;

	virtual std::unique_ptr<CodeChunk> invoke_proc(const CodeLocation  & label,
	                                               const ParameterList & params) const = 0;

	virtual std::unique_ptr<CodeChunk> invoke_proc(const CodeLocation  & label,
	                                               const ParameterList & params,
	                                               std::uintmax_t        this_param) const = 0;
};

}

#endif