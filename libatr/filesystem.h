#pragma once
#include "disk.h"
#include <string>
#include <istream>

class filesystem
{
public:
	filesystem(disk * d) : d(d) {}
	virtual ~filesystem() {};
	virtual std::string name() = 0;

	struct property
	{
		const char * name;
		disk::sector_num sector;
		size_t offset;
		size_t size;
	};

	virtual const property * properties() = 0;

	const property * find_property(const std::string & name);
	void set_property(const property * prop, const std::string & value);
	void set_property(const property * prop, int value);
	byte read_byte(disk::sector_num sector, size_t offset);
	size_t read_word(disk::sector_num sector, size_t lo_offset, size_t hi_offset);
	void write_word(disk::sector_num sector, size_t offset, size_t val);
	void write_byte(disk::sector_num sector, size_t offset, byte val);

	byte get_property_byte(const property * prop);
	std::string get_property(const property * prop);

	virtual disk::sector_num free_sector_count();

	virtual disk::sector_num get_dos_first_sector() { return 0; }
	virtual void set_dos_first_sector(disk::sector_num sector) {}

	class file
	{
	public:
		virtual bool eof() = 0;
		virtual byte read() = 0;
		virtual void write(byte b) = 0;
		void read(byte * data, size_t size);
		virtual void write(const byte * data, size_t size);
		void save(const std::string & filename);
		void import(const std::string & filename);
		virtual ~file() {};
		virtual disk::sector_num first_sector() = 0;
	};

	class dir
	{
	public:
		virtual std::string name() = 0;
		virtual size_t sec_size() = 0;
		virtual void next() = 0;
		virtual bool at_end() = 0;
		virtual file * open_file() = 0;
		virtual size_t size() = 0;
		virtual dir * open_dir();
		virtual bool  is_dir();
		virtual bool  is_deleted();
	};

	disk * get_disk();
	virtual file * create_file(char * name) = 0;
	virtual dir * root_dir() = 0;

	size_t sector_size() {
		return d->sector_size();
	}

	disk::sector_num sector_count() {
		return d->sector_count();
	}

	static bool format_atari_name(std::istream & s, char * name, size_t name_len, size_t ext_len);

protected:

	void write_sector(disk::sector_num num, byte * data)
	{
		d->write_sector(num, data);
	}

	void read_sector(disk::sector_num num, byte * data)
	{
		d->read_sector(num, data);
	}

	disk * d;
};
