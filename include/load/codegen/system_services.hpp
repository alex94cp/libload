#ifndef LOAD_CODEGEN_SYSTEMSERVICES_HPP_
#define LOAD_CODEGEN_SYSTEMSERVICES_HPP_

#include <load/export.hpp>
#include <load/codegen/common.hpp>

#include <memory>

namespace load {

class CodeChunk;

class LOAD_EXPORT SystemServices
{
public:
	virtual ~SystemServices() = default;

	virtual std::unique_ptr<CodeChunk> make_syscall(int code, const ParameterList & params) const = 0;
};

}

#endif