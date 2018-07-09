#pragma once

#include "filesystem.h"

class dos2 : public filesystem
{
public:

	dos2(disk * d);
	~dos2();

	static dos2 * format(disk * d);

	class dos2_file : public filesystem::file
	{
	public:

		dos2_file(dos2 & fs, disk::sector_num dir_sector, size_t dir_pos, int file_no, disk::sector_num first_sector, disk::sector_num sec_cnt, bool writing);
		~dos2_file();

		bool eof() override;
		byte read() override;
		void write(byte b) override;

	private:

		void write_sec(disk::sector_num next);
		bool sector_end();
		disk::sector_num sector_next();

		dos2 & fs;

		// current position
		disk::sector_num sector;
		size_t pos;
		byte * buf;

		bool writing;

		// dir
		disk::sector_num dir_sector;
		size_t           dir_pos;		// position in sector
		int				 file_no;

		disk::sector_num first_sector;
		size_t sec_cnt;					// size if not known yet
		size_t size;					// size in bytes

	};

	class dos2_dir : public filesystem::dir
	{
	public:
		dos2_dir(dos2 & fs);
		void next() override;
		bool at_end() override;
		std::string name() override;
		size_t sec_size() override;
		file * open_file() override;

	private:
		dos2 & fs;
		disk::sector_num sector;
		size_t           pos;		// position in sector
		int				 file_no;

		byte * buf;
	};

	filesystem::dir * root_dir() override;
	filesystem::file * create_file(char * name) override;

private:

	// DIR
	void dir_format();
	disk::sector_num dir_start;		// first sector of dir
	disk::sector_num dir_end;		// last sector of dir (included)
	byte * dir_buf;

	// VTOC
	
	disk::sector_num alloc_sector();
	inline void switch_sector_used(disk::sector_num sec) {
		vtoc_buf[10 + sec / 8] ^= (128 >> (sec & 7));
	}
	void vtoc_format();
	void vtoc_write();
	void vtoc_read();

	disk::sector_num vtoc_sec1;
	disk::sector_num vtoc_sec2;
	size_t vtoc_size;
	byte * vtoc_buf;
	int vtoc_dirty;
};
