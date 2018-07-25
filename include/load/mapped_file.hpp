#ifndef LOAD_MAPPEDFILE_HPP_
#define LOAD_MAPPEDFILE_HPP_

#include <load/memory.hpp>

#include <boost/iostreams/device/mapped_file.hpp>

#include <filesystem>

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