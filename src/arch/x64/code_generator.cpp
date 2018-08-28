#include <config.hpp>
#include "code_generator.hpp"
#include "../../code_chunk.hpp"

#include <load/codegen.hpp>

#include <climits>
#include <limits>
#include <memory>
#include <stdexcept>

namespace load::detail {

class X64LinuxServices final : public SystemServices
{
public:
	virtual std::unique_ptr<CodeChunk> make_syscall(int code, const ParameterList & params) const override;
};

class X64CallingConvention final : public CallingConvention
{
public:
	virtual std::unique_ptr<CodeChunk> make_prolog(unsigned int argc) const override;
	virtual std::unique_ptr<CodeChunk> make_epilog(unsigned int argc) const override;
	virtual std::unique_ptr<CodeChunk> set_return_value(std::uintmax_t value) const override;

	virtual std::unique_ptr<CodeChunk> invoke_proc(const CodeLocation  & label,
	                                               const ParameterList & params) const override;

	virtual std::unique_ptr<CodeChunk> invoke_proc(const CodeLocation  & label,
	                                               const ParameterList & params,
	                                               std::uintmax_t        this_param) const override;
};

class X64RelativeCall final : public CodeChunk
{
public:
	X64RelativeCall(const CodeLocation  & location, const ParameterList & params);

	virtual std::size_t max_size() const override;
	virtual std::size_t emit(void * location, CodeSink & code_sink) const override;

private:
	FixedCodeChunk _set_params_chunk;
	std::unique_ptr<CodeLocation> _location;
};

#ifdef LIBLOAD_ARCH_X86_64

const CodeGenerator & native_code_generator()
{
	static X64CodeGenerator x64_codegen;
	return x64_codegen;
}

#endif

namespace {

const char * PUSH_IMM32    = "\x68";
const char * MOV_RAX_IMM64 = "\x48\xb8";
const char * MOV_RCX_IMM64 = "\x48\xb9";
const char * MOV_RDX_IMM64 = "\x48\xba";
const char * MOV_RSI_IMM64 = "\x48\xbe";
const char * MOV_RDI_IMM64 = "\x48\xbf";
const char * MOV_R8_IMM64  = "\x49\xb8";
const char * MOV_R9_IMM64  = "\x49\xb9";
const char * MOV_R10_IMM64 = "\x49\xba";

void append_imm32(std::uintmax_t value, FixedCodeChunk & code_chunk)
{
	if (value > std::numeric_limits<std::uint32_t>::max())
		throw std::overflow_error("Immediate value exceeds 32 bits");

	for (std::size_t i = 0; i < sizeof(std::uint32_t); ++i)
		code_chunk.append((value >> (i * CHAR_BIT)) & UCHAR_MAX);
}

void append_imm64(std::uintmax_t value, FixedCodeChunk & code_chunk)
{
	if (value > std::numeric_limits<std::uint64_t>::max())
		throw std::overflow_error("Immediate value exceeds 64 bits");

	for (std::size_t i = 0; i < sizeof(std::uint64_t); ++i)
		code_chunk.append((value >> (i * CHAR_BIT)) & UCHAR_MAX);
}

void set_proc_parameters(const ParameterList & params, FixedCodeChunk & code_chunk)
{
	auto params_it = params.begin();
	const auto params_end = params.end();
	if (params_it == params_end) return;

	code_chunk.append(MOV_RCX_IMM64);
	append_imm64(*params_it++, code_chunk);
	if (params_it == params_end) return;

	code_chunk.append(MOV_RDX_IMM64);
	append_imm64(*params_it++, code_chunk);
	if (params_it == params_end) return;

	code_chunk.append(MOV_R8_IMM64);
	append_imm64(*params_it++, code_chunk);
	if (params_it == params_end) return;

	code_chunk.append(MOV_R9_IMM64);
	append_imm64(*params_it++, code_chunk);
	if (params_it == params_end) return;

	for (auto params_rit = params.rbegin(); params_rit.base() != params_it; ++params_rit) {
		const auto value = static_cast<std::uint64_t>(*params_rit);
		
		code_chunk.append(PUSH_IMM32);
		append_imm32((value >>  0) & std::uint32_t(~0), code_chunk);

		code_chunk.append(PUSH_IMM32);
		append_imm32((value >> 32) & std::uint32_t(~0), code_chunk);
	}
}

void set_syscall_parameters(const ParameterList & params, FixedCodeChunk & code_chunk)
{
	auto params_it = params.begin();
	const auto params_end = params.end();
	if (params_it == params_end) return;

	code_chunk.append(MOV_RDI_IMM64);
	append_imm64(*params_it++, code_chunk);
	if (params_it == params_end) return;

	code_chunk.append(MOV_RSI_IMM64);
	append_imm64(*params_it++, code_chunk);
	if (params_it == params_end) return;

	code_chunk.append(MOV_RDX_IMM64);
	append_imm64(*params_it++, code_chunk);
	if (params_it == params_end) return;

	code_chunk.append(MOV_R10_IMM64);
	append_imm64(*params_it++, code_chunk);
	if (params_it == params_end) return;

	code_chunk.append(MOV_R8_IMM64);
	append_imm64(*params_it++, code_chunk);
	if (params_it == params_end) return;

	code_chunk.append(MOV_R9_IMM64);
	append_imm64(*params_it++, code_chunk);
	if (params_it == params_end) return;

	throw std::invalid_argument("Too many syscall parameters");
}

}

const CallingConvention * X64CodeGenerator::get_calling_convention() const
{
	static const X64CallingConvention x64_cconv;
	return &x64_cconv;
}

const CallingConvention * X64CodeGenerator::get_calling_convention(std::string_view hint) const
{
	if (hint == "cdecl" || hint == "stdcall")
		return get_calling_convention();

	return nullptr;
}

const SystemServices * X64CodeGenerator::get_system_services(std::string_view uname) const
{
	if (uname == "linux") {
		static const X64LinuxServices linux_services;
		return &linux_services;
	}

	return nullptr;
}

std::unique_ptr<CodeChunk> X64LinuxServices::make_syscall(int code, const ParameterList & params) const
{
	auto code_chunk = std::make_unique<FixedCodeChunk>();
	set_syscall_parameters(params, *code_chunk);

	code_chunk->append(MOV_RAX_IMM64);
	append_imm64(code, *code_chunk);

	code_chunk->append("\x0f\x05" /* syscall */);
	return code_chunk;
}

std::unique_ptr<CodeChunk> X64CallingConvention::make_prolog(unsigned int) const
{
	return std::make_unique<CodeChunkLiteral>(
		"\x55"         // push rbp
		"\x48\x89\xe5" // mov rbp, rsp
	);
}

std::unique_ptr<CodeChunk> X64CallingConvention::make_epilog(unsigned int) const
{
	return std::make_unique<CodeChunkLiteral>(
		"\x48\x89\xec" // mov rsp, rbp
		"\x5d"         // pop rbp
		"\xc3"         // ret
	);
}

std::unique_ptr<CodeChunk> X64CallingConvention::set_return_value(std::uintmax_t value) const
{
	auto code_chunk = std::make_unique<FixedCodeChunk>();
	code_chunk->append(MOV_RAX_IMM64);
	append_imm64(value, *code_chunk);
	return code_chunk;
}

std::unique_ptr<CodeChunk> X64CallingConvention::invoke_proc(const CodeLocation  & location,
                                                             const ParameterList & params) const
{
	return std::make_unique<X64RelativeCall>(location, params);
}

std::unique_ptr<CodeChunk> X64CallingConvention::invoke_proc(const CodeLocation  & location,
                                                             const ParameterList & params,
                                                             std::uintmax_t        this_param) const
{
	ParameterList proc_params;
	proc_params.push_back(this_param);
	std::copy(params.begin(), params.end(), std::back_inserter(proc_params));
	return std::make_unique<X64RelativeCall>(location, std::move(proc_params));
}

X64RelativeCall::X64RelativeCall(const CodeLocation & location, const ParameterList & params)
	: _location { location.clone() }
{
	set_proc_parameters(params, _set_params_chunk);
}

std::size_t X64RelativeCall::max_size() const
{
	return _set_params_chunk.max_size() + 1 + sizeof(std::uint32_t);
}

std::size_t X64RelativeCall::emit(void * location, CodeSink & code_sink) const
{
	std::size_t chunk_size = _set_params_chunk.emit(location, code_sink);
	const auto call_location = static_cast<char *>(location) + chunk_size;

	FixedCodeChunk call_chunk;
	call_chunk.append('\xe8' /* call rel32 */);
	append_imm32(_location->distance_from(call_location) - 5, call_chunk);
	chunk_size += call_chunk.emit(call_location, code_sink);
	return chunk_size;
}

}