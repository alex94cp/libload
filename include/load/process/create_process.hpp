#ifndef LOAD_PROCESS_CREATEPROCESS_HPP_
#define LOAD_PROCESS_CREATEPROCESS_HPP_

#include <load/export.hpp>

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace load {

class Process;
class MemoryBuffer;
class ModuleProvider;

using FileHandle = void *;

struct ProcessOptions
{
	enum {
		InheritHandles  = 1 << 0,
		CreateSuspended = 1 << 1,
	};

	using FilePath = std::filesystem::path;
	using DirectoryPath = std::filesystem::path;

	using EnvironmentBlock = std::vector <
		std::pair<std::string, std::string>
	>;

	unsigned int                    flags;
	std::optional<DirectoryPath>    cwd;
	std::optional<EnvironmentBlock> env;
	std::optional<FilePath>         egg_path;
	std::optional<FileHandle>       stdin_handle;
	std::optional<FileHandle>       stdout_handle;
	std::optional<FileHandle>       stderr_handle;
};

LOAD_EXPORT std::unique_ptr<Process> create_process(const MemoryBuffer   & image_data,
                                                    const ProcessOptions & options,
                                                    ModuleProvider       & module_provider);

}

#endif