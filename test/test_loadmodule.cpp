#define BOOST_TEST_MODULE LoadModule
#include <boost/test/unit_test.hpp>

#include <load/mapped_file.hpp>
#include <load/memory_module.hpp>

#include <memory>

struct ModuleTest
{
	ModuleTest()
		: _file { "sample_module.llm" }
	{}

	load::MappedFile _file;
	std::shared_ptr<load::Module> _module;
};

BOOST_FIXTURE_TEST_CASE(load_module, ModuleTest)
{
	BOOST_REQUIRE_NO_THROW({
		_module = load::load_module(_file);
	});
}

BOOST_FIXTURE_TEST_CASE(get_proc, ModuleTest)
{
	_module = load::load_module(_file);
	const auto sample_proc = _module->get_proc<int()>("sample_proc");
	BOOST_CHECK_NE(sample_proc, nullptr);
	BOOST_CHECK_EQUAL(sample_proc(), 123);
}

BOOST_FIXTURE_TEST_CASE(get_data, ModuleTest)
{
	_module = load::load_module(_file);
	const int * sample_data = _module->get_data<int>("sample_data");
	BOOST_CHECK_NE(sample_data, nullptr);
	BOOST_CHECK_EQUAL(*sample_data, 123);
}
