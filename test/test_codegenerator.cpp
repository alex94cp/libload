#define BOOST_TEST_MODULE CodeGenerator
#include <boost/test/unit_test.hpp>

#include "../src/code_chunk.hpp"
#include "../src/memory_block.hpp"

#include <load/codegen.hpp>
#include <load/memory.hpp>
#include <load/process.hpp>

#include <utility>

using namespace load;

struct CodeGeneratorTest
{
	CodeGeneratorTest()
		: _code_generator { &current_process().code_generator() }
	{}

	const CodeGenerator * _code_generator;
};

detail::CodeBlock make_stub_fn(const CallingConvention & calling_convention)
{
	detail::CodeBlock code_block;
	code_block.add(calling_convention.make_prolog(0));
	code_block.add(calling_convention.set_return_value(0));
	code_block.add(calling_convention.make_epilog(0));
	return code_block;
}

template <typename Fn, typename... Args>
auto invoke_code_block(const detail::CodeBlock & code_block, Args && ...args)
{
	const std::size_t block_size = code_block.max_size();
	MemoryManager & memory_manager = current_process().memory_manager();
	detail::OwnedMemoryBlock mem_block { memory_manager, memory_manager.allocate(0, block_size), block_size };
	memory_manager.commit(mem_block.data(), mem_block.size());
	
	code_block.emit(mem_block.data(), detail::MemoryBufferCodeSink(mem_block));
	const int mem_access = MemoryManager::ReadAccess | MemoryManager::ExecuteAccess;
	memory_manager.set_access(mem_block.data(), mem_block.size(), mem_access);
	return reinterpret_cast<Fn>(mem_block.data())(std::forward<Args>(args)...);
}

BOOST_FIXTURE_TEST_CASE(stdcall_fn, CodeGeneratorTest)
{
	const auto stdcall_cconv = _code_generator->get_calling_convention("stdcall");
	if (stdcall_cconv == nullptr) return;

	const auto stub_fn = make_stub_fn(*stdcall_cconv);
	BOOST_REQUIRE_NO_THROW({ invoke_code_block<int (__stdcall *)()>(stub_fn); });
}

BOOST_FIXTURE_TEST_CASE(cdecl_fn, CodeGeneratorTest)
{
	const auto cdecl_cconv = _code_generator->get_calling_convention("cdecl");
	if (cdecl_cconv == nullptr) return;

	const auto stub_fn = make_stub_fn(*cdecl_cconv);
	BOOST_REQUIRE_NO_THROW({ invoke_code_block<int (__cdecl *)()>(stub_fn); });
}