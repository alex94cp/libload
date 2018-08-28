#include "system_module.hpp"
#include "../../module_provider.hpp"

#include <memory>

namespace load {

using namespace detail;

ModuleProvider & system_module_provider =
	make_module_provider([] (const std::string & name) {
		std::shared_ptr<SystemModule> module_sp;
		if (const HMODULE module_handle = LoadLibraryA(name.c_str()))
			module_sp = std::make_shared<SystemModule>(module_handle);
		return module_sp;
	});

namespace detail {

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

} }