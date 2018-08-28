#ifndef LOAD_MODULE_MODULEPROVIDER_HPP_
#define LOAD_MODULE_MODULEPROVIDER_HPP_

#include <load/export.hpp>

#include <memory>
#include <string_view>

namespace load {

class Module;

class ModuleProvider
{
public:
	virtual ~ModuleProvider() = default;
	virtual std::shared_ptr<Module> get_module(std::string_view name) = 0;
};

LOAD_EXPORT extern ModuleProvider & system_module_provider;

}

#endif