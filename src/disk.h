#pragma once

#define _CRT_SECURE_NO_WARNINGS
#include <cstring>
#include <stdint.h>
#include <string>

typedef uint8_t byte;

#define set_word(buf, idx, n) buf[idx] = byte((n) & 255); buf[idx+1] = byte((n) >> 8);
#define read_word(buf, idx) (buf[idx] + buf[idx+1] * 256)

class disk
{
public:

	typedef size_t sector_num;

	disk(size_t sector_size, sector_num sector_count) : s_size(sector_size), s_count(sector_count)
	{
		data = new byte[byte_size()];
		memset(data, 0, byte_size());
	}

	sector_num sector_count() const {
		return s_count;
	}

	size_t sector_size() const {
		return s_size;
	}

	size_t byte_size() const {
		return (s_count - 3) * s_size + 3 * 128;
	}

	size_t sector_size(sector_num num) const {
		return (num <= 3) ? 128 : s_size;
	}

	void write_sector(sector_num num, byte * data)
	{
		memcpy(sector_ptr(num), data, sector_size(num));
	}

	void read_sector(sector_num num, byte * data)
	{
		memcpy(data, sector_ptr(num), sector_size(num));
	}

	static disk * load(const std::string & filename);
	void save(const std::string & filename);

	void install_boot(const std::string & filename);
	void save_boot(const std::string & filename);


	struct sector {
		disk & d;
		sector_num num;
		bool dirty;
		byte * buf;

		sector(disk & d, sector_num num);
		~sector();
		void write();

	};

	sector * get_sector(sector_num num);

private:
	byte * sector_ptr(size_t num) {
		return &data[(num <= 3) ? (num - 1) * 128 : 3 * 128 + (num - 4) * s_size];
	}

	size_t s_size;
	sector_num s_count;
	byte * data;

};
