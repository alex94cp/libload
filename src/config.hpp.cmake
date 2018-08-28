#ifndef LOAD_SRC_CONFIG_HPP_
#define LOAD_SRC_CONFIG_HPP_

#if defined(_M_AMD64) || defined(__amd64__)
#	define LIBLOAD_ARCH_X86_64 1
#elif defined(_M_IX64) || defined(__i386__)
#	define LIBLOAD_ARCH_X86    1
#endif

#cmakedefine LIBLOAD_ENABLE_ARCH_X86
#cmakedefine LIBLOAD_ENABLE_ARCH_X86_64

#cmakedefine LIBLOAD_ENABLE_FORMAT_PE32
#cmakedefine LIBLOAD_ENABLE_FORMAT_PE64

#endif