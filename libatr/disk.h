#pragma once

#define _CRT_SECURE_NO_WARNINGS
#include <cstring>
#include <stdint.h>
#include <string>

typedef uint8_t byte;
typedef uint16_t word;

#define poke_word(buf, idx, n) buf[idx] = byte((n) & 255); buf[idx+1] = byte((n) >> 8);
#define peek_word(buf, idx) (buf[idx] + buf[idx+1] * 256)

class disk
{
public:

	typedef size_t sector_num;

	disk(size_t sector_size, sector_num sector_count);

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

	static disk * load(const std::string & filename);
	void save(const std::string & filename);

	void install_boot(const std::string & filename);
	void save_boot(const std::string & filename);


	struct sector {
		int next, prev;
		sector_num num;
		bool dirty;
		byte * buf;

		void init(disk & d);
		sector();
		~sector();

		void poke(size_t offset, byte value)
		{
			buf[offset] = value;
			dirty = true;
		}

		void dpoke(size_t offset, word value)
		{
			poke_word(buf, offset, value)
			dirty = true;
		}

		void copy(size_t offset, const char * ptr, size_t size);

		inline byte peek(size_t offset) const { return buf[offset]; }
		word dpeek(size_t offset) const;
		byte operator[](size_t offset) const { return buf[offset]; }

		void set(size_t offset, size_t size, byte b);

	};

	sector * get_sector(sector_num num, bool init);
	sector * get_sector(sector_num num);
	sector * init_sector(sector_num num);

	void flush();
	void flush(sector & s);

	void write_sector(sector_num num, byte * data);
	void read_sector(sector_num num, byte * data)
	{
		memcpy(data, sector_ptr(num), sector_size(num));
	}

	byte read_byte(sector_num sector, size_t offset);
	word read_word(sector_num sector, size_t offset);
	word read_word(sector_num sector, size_t lo_offset, size_t hi_offset);

	void write_byte(sector_num sector, size_t offset, byte val);
	void write_word(sector_num sector, size_t offset, word val);
	void write_word(sector_num sector, size_t lo_offset, size_t hi_offset, word val);

private:

	byte * sector_ptr(size_t num) {
		return &data[(num <= 3) ? (num - 1) * 128 : 3 * 128 + (num - 4) * s_size];
	}

	size_t s_size;
	sector_num s_count;
	byte * data;

	enum {
		cache_size = 4
	};

	sector cache[cache_size];
	int last;
	void print_cache();
};
