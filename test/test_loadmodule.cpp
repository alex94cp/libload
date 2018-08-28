#define BOOST_TEST_MODULE LoadModule
#include <boost/test/unit_test.hpp>

#include <load/memory.hpp>
#include <load/module.hpp>

#include <memory>

using namespace load;

struct ModuleTest
{
	ModuleTest()
		: _file { "sample_module.llm" }
	{}

	MappedFile _file;
};

BOOST_FIXTURE_TEST_CASE(load_module_from_memory, ModuleTest)
{
	BOOST_REQUIRE_NO_THROW({
		auto module = load::load_module(_file);
		BOOST_REQUIRE_NE(module, nullptr);
	});
}

BOOST_FIXTURE_TEST_CASE(get_proc, ModuleTest)
{
	auto module = load::load_module(_file);
	const auto sample_proc = module->get_proc<int()>("sample_proc");
	BOOST_CHECK_NE(sample_proc, nullptr);
	BOOST_CHECK_EQUAL(sample_proc(), 123);
}

BOOST_FIXTURE_TEST_CASE(get_data, ModuleTest)
{
	auto module = load::load_module(_file);
	const int * sample_data = module->get_data<int>("sample_data");
	BOOST_CHECK_NE(sample_data, nullptr);
	BOOST_CHECK_EQUAL(*sample_data, 123);
}

#if defined(_MSC_VER) || defined(__DMC__)

BOOST_FIXTURE_TEST_CASE(module_seh_handler, ModuleTest)
{
	auto module = load::load_module(_file);
	const auto sample_seh = module->get_proc<int()>("sample_seh");
	BOOST_CHECK_EQUAL(sample_seh(), 123);
}

#endif