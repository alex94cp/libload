#include "image.hpp"
#include <peplus/local_buffer.hpp>
#include <peplus/virtual_image.hpp>

#include <cstdint>

using peplus::local_buffer;

extern "C" {

__declspec(dllexport) int __libload_init(char * image_base)
{
	peplus::LocalBuffer image_buffer { image_base };
	if constexpr (sizeof(void *) == sizeof(std::uint32_t)) {
		peplus::VirtualImage32<local_buffer> image { image_buffer };
		return load::detail::initialize_dll_direct(image, image_base);
	} else if constexpr (sizeof(void *) == sizeof(std::uint64_t)) {
		peplus::VirtualImage64<local_buffer> image { image_buffer };
		return load::detail::initialize_dll_direct(image, image_base);
	}
}

__declspec(dllexport) void __libload_cleanup(char * image_base)
{
	peplus::LocalBuffer image_buffer { image_base };
	if constexpr (sizeof(void *) == sizeof(std::uint32_t)) {
		peplus::VirtualImage32<local_buffer> image { image_base };
		load::detail::deinitialize_dll_direct(image, image_base);
	} else if constexpr (sizeof(void *) == sizeof(std::uint64_t)) {
		peplus::VirtualImage64<local_buffer> image { image_base };
		load::detail::deinitialize_dll_direct(image, image_base);
	}
}

}