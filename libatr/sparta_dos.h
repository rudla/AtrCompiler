#pragma once

#include "filesystem.h"

class sparta_dos : public filesystem
{
public:

	enum dir_entry {
		dir_first_sector = 1,
		dir_file_size = 3,  // 3 bytes
		dir_filename = 6,
		dir_date = 17,
		dir_time = 20,
		dir_entry_size = 23
	};

	sparta_dos(disk * d);
	~sparta_dos();

	std::string name() override;
	const property * properties();
	disk::sector_num free_sector_count() override;
	disk::sector_num get_dos_first_sector() override;
	void set_dos_first_sector(disk::sector_num sector) override;

	static filesystem * format(disk * d);
	static bool detect(disk * d);

	class sparta_dos_file : public filesystem::file
	{
		friend class sparta_dos_dir;
	public:

		sparta_dos_file(sparta_dos & fs, disk::sector_num first_map, size_t size, bool writing);
		~sparta_dos_file();

		bool eof() override;
		byte read() override;
		void write(byte b) override;
		disk::sector_num first_sector() override;

		sparta_dos & filesystem();

		size_t size();

		void set_size(size_t size);

	private:
		void write_sec(disk::sector_num next);
		bool sector_end();
		disk::sector_num sector_next();

		sparta_dos & fs;

		disk::sector_num first_map;
		disk::sector_num sector_map;
		byte * map_buf;

		size_t		map_offset;				// number of sector in sector map (initialized to 2 for first sector)
	
		// current position
		disk::sector_num sector;
		size_t pos;
		byte * data_buf;

		bool writing;

		size_t byte_size;					// size in bytes
		size_t byte_pos;
	};

	class sparta_dos_dir : public filesystem::dir
	{
	public:
		sparta_dos_dir(sparta_dos_file * file);
		~sparta_dos_dir();
		void next() override;
		bool at_end() override;
		std::string name() override;
		size_t sec_size() override;
		file * open_file() override;
		size_t size() override;
		bool is_dir() override;
		bool is_deleted() override;
		dir * open_dir() override;

	private:
		disk::sector_num first_sector();
		sparta_dos_file * f;
		byte buf[dir_entry_size];
	};

	filesystem::dir * root_dir() override;
	filesystem::file * create_file(char * name) override;
	dir * open_dir(disk::sector_num sector);

protected:

	// DIR
	void dir_format();
	disk::sector_num dir_sector;		// first sector of dir
	disk::sector_num free_count;
	byte * dir_buf;

	// VTOC

	inline void switch_sector_used(disk::sector_num sec) {
		vtoc_buf[10 + sec / 8] ^= (128 >> (sec & 7));
	}

	virtual disk::sector_num alloc_sector();
	virtual void vtoc_format();
	virtual void vtoc_read();
	virtual void vtoc_write();

	disk::sector_num bitmap_sec;
	size_t bitmap_sec_count;
	byte * vtoc_buf;
	int vtoc_dirty;
};
