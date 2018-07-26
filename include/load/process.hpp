#ifndef LOAD_PROCESS_HPP_
#define LOAD_PROCESS_HPP_

#include <load/export.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace load {

class MemoryBuffer;
class MemoryManager;
class ModuleProvider;

using FileHandle = void *;
using ProcessHandle = void *;
using ProcessId = unsigned long;

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

class LOAD_EXPORT Process
{
public:
	virtual ~Process() = default;
	virtual ProcessId process_id() const = 0;
	virtual ProcessHandle native_handle() = 0;
	virtual MemoryManager & memory_manager() = 0;
	virtual const MemoryManager & memory_manager() const = 0;

	virtual void register_exception_table(std::uintptr_t base_address,
	                                      void         * exception_table,
	                                      std::size_t    table_size) = 0;
	
	virtual void deregister_exception_table(void * exception_table) = 0;
};

LOAD_EXPORT Process & current_process();
LOAD_EXPORT std::unique_ptr<Process> open_process(ProcessId process_id);
LOAD_EXPORT std::unique_ptr<Process> create_process(const MemoryBuffer   & image_data,
                                                    const ProcessOptions & options,
                                                    const ModuleProvider & module_provider);

}

#endif