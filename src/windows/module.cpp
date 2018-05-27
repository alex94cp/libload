#include <load/module.hpp>

#include <windows.h>

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

class SystemModuleProvider final : public ModuleProvider
{
public:
	virtual std::shared_ptr<Module> get_module(std::string_view name) const override;
};

const ModuleProvider & system_module_provider = SystemModuleProvider {};

SystemModule::SystemModule(HMODULE handle)
	: _handle { handle } {}

SystemModule::SystemModule(SystemModule && other)
	: _handle { other._handle }
{
	other._handle = 0;
}

SystemModule::~SystemModule()
{
	if (_handle != 0)
		FreeLibrary(_handle);
}

ProcPtr SystemModule::get_proc_address(std::string_view name) const
{
	const DataPtr data_addr = get_data_address(name);
	return static_cast<ProcPtr>(data_addr);
}

DataPtr SystemModule::get_data_address(std::string_view name) const
{
	const std::string name_s { name };
	return GetProcAddress(_handle, name_s.c_str());
}

std::shared_ptr<Module> SystemModuleProvider::get_module(std::string_view name) const
{
	const std::string name_s { name };
	const HMODULE module_handle = LoadLibraryA(name_s.c_str());
	if (!module_handle) return nullptr;

	return std::make_shared<SystemModule>(module_handle);
}

}