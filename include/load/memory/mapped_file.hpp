#ifndef LOAD_MEMORY_MAPPEDFILE_HPP_
#define LOAD_MEMORY_MAPPEDFILE_HPP_

#include <load/memory/memory_buffer.hpp>

#include <cstddef>
#include <filesystem>

#include <boost/iostreams/device/mapped_file.hpp>

namespace load {

class LOAD_EXPORT MappedFile final : public MemoryBuffer
{
public:
	explicit MappedFile(const std::filesystem::path & path);

	virtual std::size_t read(std::size_t offset, std::size_t size,
	                         void * into_buffer) const override;

private:
	boost::iostreams::mapped_file_source _mm_file;
};

}

#endif