#ifndef LOAD_MODULE_MODULE_HPP_
#define LOAD_MODULE_MODULE_HPP_

#include <load/export.hpp>

#include <string_view>

namespace load {

class MemoryBuffer;

using DataPtr = const void *;
using ProcPtr = void (*)(...);

class Module
{
public:
	virtual ~Module() = default;

	template <typename T>
	T * get_data(std::string_view name);

	template <typename T>
	const T * get_data(std::string_view name) const;

	template <typename Fn>
	Fn * get_proc(std::string_view name) const;

protected:
	virtual DataPtr get_data_address(std::string_view name) const = 0;
	virtual ProcPtr get_proc_address(std::string_view name) const = 0;
};

template <typename T>
T * Module::get_data(std::string_view name)
{
	return const_cast<T *>(const_cast<const Module *>(this)->get_data<T>(name));
}

template <typename T>
const T * Module::get_data(std::string_view name) const
{
	return static_cast<const T *>(get_data_address(name));
}

template <typename Fn>
Fn * Module::get_proc(std::string_view name) const
{
	return reinterpret_cast<Fn *>(get_proc_address(name));
}

}

#endif