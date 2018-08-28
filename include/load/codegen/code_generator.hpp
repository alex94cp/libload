#ifndef LOAD_CODEGEN_CODEGENERATOR_HPP_
#define LOAD_CODEGEN_CODEGENERATOR_HPP_

#include <load/export.hpp>

#include <string_view>

namespace load {

class CallingConvention;
class SystemServices;

class LOAD_EXPORT CodeGenerator
{
public:
	virtual ~CodeGenerator() = default;

	virtual const CallingConvention * get_calling_convention() const = 0;
	virtual const CallingConvention * get_calling_convention(std::string_view hint) const = 0;

	virtual const SystemServices * get_system_services(std::string_view uname) const = 0;
};

}

#endif