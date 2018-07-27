#include <load/mapfile.hpp>

#include <algorithm>

namespace load {

MappedFile::MappedFile(const std::filesystem::path & path)
	: _mm_file { path.string() } {}

std::size_t MappedFile::read(std::size_t offset, std::size_t size, void * into_buffer) const
{
	const std::size_t bytes_to_read = std::min(_mm_file.size() - offset, size);
	std::copy_n(_mm_file.data() + offset, bytes_to_read, static_cast<char *>(into_buffer));
	return bytes_to_read;
}

}