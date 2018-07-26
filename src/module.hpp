#ifndef LOAD_SRC_MODULE_HPP_
#define LOAD_SRC_MODULE_HPP_

#include <config.hpp>
#include "memory.hpp"

#include <load/module.hpp>

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>

namespace load::detail {

struct owned_module
{
	using memory_policy = owned_memory;

	template <class Fn>
	static void dispose(Fn && dispose_fn)
	{
		std::forward<Fn>(dispose_fn)();
	}
};

struct borrowed_module
{
	using memory_policy = borrowed_memory;

	template <class Fn>
	static void dispose(Fn &&) {}
};

template <class Fn>
class ModuleProviderFn final : public ModuleProvider
{
public:
	ModuleProviderFn(Fn fn);

	virtual std::shared_ptr<Module> get_module(std::string_view name) const override;

private:
	Fn _fn;
};

template <template <class...> class Map = std::unordered_map>
class CachingModuleProvider final : public ModuleProvider
{
public:
	using module_map = Map<std::string, std::shared_ptr<Module>>;

	explicit CachingModuleProvider(const ModuleProvider & mod_provider);

	module_map module_cache() &&;
	const module_map & module_cache() const &;

	virtual std::shared_ptr<Module> get_module(std::string_view name) const override;

private:
	const ModuleProvider * _module_provider;
	mutable module_map     _module_cache;
};

template <class Fn>
auto make_module_provider(Fn && fn)
{
	return ModuleProviderFn(std::forward<Fn>(fn));
}

template <class Fn>
auto make_caching_module_provider(Fn load_fn)
{
	return make_module_provider([do_load_module=std::move(load_fn)] (std::string_view name) {
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
std::shared_ptr<Module> ModuleProviderFn<Fn>::get_module(std::string_view name) const
{
	return _fn(name);
}

template <template <class...> class Map>
CachingModuleProvider<Map>::CachingModuleProvider(const ModuleProvider & mod_provider)
	: _module_provider { &mod_provider } {}

template <template <class...> class Map>
auto CachingModuleProvider<Map>::module_cache() && -> module_map
{
	return std::move(_module_cache);
}

template <template <class...> class Map>
auto CachingModuleProvider<Map>::module_cache() const & -> const module_map &
{
	return _module_cache;
}

template <template <class...> class Map>
std::shared_ptr<Module> CachingModuleProvider<Map>::get_module(std::string_view name) const
{
	std::string name_s { name };
	const auto module_iter = _module_cache.find(name_s);
	if (module_iter != _module_cache.end())
		return module_iter->second;

	std::shared_ptr<Module> module_sp = _module_provider->get_module(name_s);
	_module_cache.insert_or_assign(module_iter, std::move(name_s), module_sp);
	return module_sp;
}

}

#endif