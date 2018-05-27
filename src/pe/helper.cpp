#include "helper.hpp"

namespace load::detail {

extern "C" {

LOAD_EXPORT DWORD __libload_helper(const PEHelperParams * params)
{
	return params && params->fwd_proc(params->hinstance,
	                                  params->fdw_reason,
	                                  params->lpv_reserved);
}

}

}