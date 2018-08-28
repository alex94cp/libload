#include "module_provider.hpp"

namespace load::detail {

ModuleCache::ModuleCache(ModuleProvider & mod_provider)
	: _module_provider { &mod_provider } {}

std::shared_ptr<Module> ModuleCache::get_module(std::string_view name)
{
	std::string name_s { name };
	const auto module_iter = _module_entries.find(name_s);
	if (module_iter != _module_entries.end())
		return module_iter->second;

	std::shared_ptr<Module> module_sp = _module_provider->get_module(name_s);
	_module_entries.insert_or_assign(module_iter, std::move(name_s), module_sp);
	return module_sp;
}

}