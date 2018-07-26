#include "../module.hpp"

#include <windows.h>

#include <memory>
#include <string>
#include <system_error>

namespace load {

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

const ModuleProvider & system_module_provider =
	detail::make_caching_module_provider([] (const std::string & name) {
		std::shared_ptr<SystemModule> module_sp;
		if (const HMODULE module_handle = LoadLibraryA(name.c_str()))
			module_sp = std::make_shared<SystemModule>(module_handle);
		return module_sp;
	});

SystemModule::SystemModule(HMODULE handle)
	: _handle { handle } {}

SystemModule::SystemModule(SystemModule && other)
	: _handle { other._handle }
{
	other._handle = nullptr;
}

SystemModule::~SystemModule()
{
	if (_handle != nullptr)
		FreeLibrary(_handle);
}

ProcPtr SystemModule::get_proc_address(std::string_view name) const
{
	const DataPtr data_addr = get_data_address(name);
	return reinterpret_cast<ProcPtr>(data_addr);
}

DataPtr SystemModule::get_data_address(std::string_view name) const
{
	const std::string name_s { name };
	return GetProcAddress(_handle, name_s.c_str());
}

}