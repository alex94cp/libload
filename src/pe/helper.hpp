#ifndef LOAD_SRC_PE_HELPER_HPP_
#define LOAD_SRC_PE_HELPER_HPP_

#include <load/export.hpp>

#include <cstdint>

namespace load::detail {

using LPVOID = void *;
using HINSTANCE = void *;
using DWORD = std::uint32_t;

using PEFwdProc = DWORD (__stdcall *)(HINSTANCE, DWORD, LPVOID);

struct PEHelperParams
{
	PEFwdProc fwd_proc;
	HINSTANCE hinstance;
	DWORD     fdw_reason;
	LPVOID    lpv_reserved;
};

}

#endif