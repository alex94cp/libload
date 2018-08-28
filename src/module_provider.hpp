#ifndef LOAD_SRC_MODULEPROVIDER_HPP_
#define LOAD_SRC_MODULEPROVIDER_HPP_

#include <config.hpp>
#include <load/module.hpp>

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>

namespace load::detail {

template <class Fn>
class ModuleProviderFn final : public ModuleProvider
{
public:
	ModuleProviderFn(Fn fn);

	virtual std::shared_ptr<Module> get_module(std::string_view name) override;

private:
	Fn _fn;
};

class ModuleCache final : public ModuleProvider
{
public:
	explicit ModuleCache(ModuleProvider & mod_provider);

	virtual std::shared_ptr<Module> get_module(std::string_view name) override;

private:
	using module_map = std::unordered_map<std::string, std::shared_ptr<Module>>;

	ModuleProvider * _module_provider;
	module_map       _module_entries;
};

template <class Fn>
auto make_module_provider(Fn && load_fn)
{
	return ModuleProviderFn([do_load_module=std::move(load_fn)] (std::string_view name) {
		static std::unordered_map<std::string, std::weak_ptr<Module>> module_cache;

		std::string name_s { name };
		const auto module_iter = module_cache.find(name_s);
		if (module_iter != module_cache.end()) {
			if (auto module_sp = module_iter->second.lock())
				return module_sp;
		}

		std::shared_ptr<Module> module_sp = do_load_module(name_s);
		module_cache.insert_or_assign(module_iter, std::move(name_s), module_sp);
		return module_sp;
	});
}

template <class Fn>
ModuleProviderFn<Fn>::ModuleProviderFn(Fn fn)
	: _fn { std::move(fn) } {}

template <class Fn>
std::shared_ptr<Module> ModuleProviderFn<Fn>::get_module(std::string_view name)
{
	return _fn(name);
}

}

#endif