#pragma once
#include <string>
#include <cstdint>
#include <algorithm>

#if defined(__linux__)
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace io
{
	struct View
	{
		const uint8_t* data = nullptr;
		size_t size = 0;
	};

#if defined(__linux__)

	struct File
	{
		int fd = -1;
		const uint8_t* data = nullptr;
		size_t size = 0;
	};

	inline bool open(const std::string& path, File& out)
	{
		out.fd = ::open(path.c_str(), O_RDONLY);
		if (out.fd < 0) return false;

		struct stat st;
		if (fstat(out.fd, &st) != 0)
		{
			::close(out.fd);
			return false;
		}

		if (st.st_size <= 0)
		{
			::close(out.fd);
			return false;
		}

		out.size = (size_t)st.st_size;

		void* ptr = mmap(
			nullptr,
			out.size,
			PROT_READ,
			MAP_PRIVATE,
			out.fd,
			0
		);

		if (ptr == MAP_FAILED)
		{
			::close(out.fd);
			return false;
		}

		out.data = (const uint8_t*)ptr;
		return true;
	}

	inline void close(File& f)
	{
		if (f.data)
			munmap((void*)f.data, f.size);

		if (f.fd >= 0)
			::close(f.fd);

		f.data = nullptr;
		f.fd = -1;
		f.size = 0;
	}

	inline View view(const File& f)
	{
		return { f.data, f.size };
	}

#else
	#error "mmap core currently implemented for Linux only"
#endif
}