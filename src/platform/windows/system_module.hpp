#ifndef LOAD_SRC_PLATFORM_WINDOWS_SYSTEMMODULE_HPP_
#define LOAD_SRC_PLATFORM_WINDOWS_SYSTEMMODULE_HPP_

#include <load/module/module.hpp>

#include <windows.h>

namespace load::detail {

class SystemModule final : public Module
{
public:
	explicit SystemModule(HMODULE handle);
	SystemModule(SystemModule && other);
	virtual ~SystemModule();

protected:
	virtual ProcPtr get_proc_address(std::string_view name) const override;
	virtual DataPtr get_data_address(std::string_view name) const override;

private:
	HMODULE _handle;
};

}

#endif