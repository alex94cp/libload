#ifndef LOAD_SRC_ARCH_X64_CODEGENERATOR_HPP_
#define LOAD_SRC_ARCH_X64_CODEGENERATOR_HPP_

#include <load/codegen/code_generator.hpp>

namespace load::detail {

class X64CodeGenerator final : public CodeGenerator
{
public:
	virtual const CallingConvention * get_calling_convention() const override;
	virtual const CallingConvention * get_calling_convention(std::string_view hint) const override;
	
	virtual const SystemServices * get_system_services(std::string_view uname) const override;
};

}

#endif