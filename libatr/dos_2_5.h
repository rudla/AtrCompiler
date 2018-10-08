#pragma once

#include "filesystem.h"

class dos25 : public filesystem
{
public:

	dos25(disk * d);
	~dos25();

	//static disk::sector_num dir_start;
	//static disk::sector_num dir_size;

	std::string name() override;

	const property * properties() override;

	static filesystem * format(disk * d);
	static bool detect(disk * d);

	disk::sector_num free_sector_count() override;

	disk::sector_num get_dos_first_sector() override;
	void set_dos_first_sector(disk::sector_num sector) override;

	class dos2_file : public filesystem::file
	{
	public:

		dos2_file(dos25 & fs, disk::sector_num dir_sector, size_t dir_pos, int file_no, disk::sector_num first_sec, disk::sector_num sec_cnt, bool writing);
		~dos2_file();

		bool eof() override;
		byte read() override;
		void write(byte b) override;
		disk::sector_num first_sector() override;

	protected:

		void write_sec(disk::sector_num next);
		bool sector_end();
		disk::sector_num sector_next();
		bool next_sector();

		dos25 & fs;

		// current position
		disk::sector_num sector;
		size_t pos;
		byte * buf;

		bool writing;


		// dir
		disk::sector_num dir_sector;
		size_t           dir_pos;		// position in sector
		int				 file_no;
		
		disk::sector_num first_sec;
		size_t sec_cnt;					// size if not known yet
		size_t size;					// size in bytes

		bool   created_by_dos2;
	};

	class dos2_dir : public filesystem::dir
	{
	public:
		dos2_dir(dos25 & fs, disk::sector_num sector);
		void next() override;
		bool at_end() override;
		std::string name() override;
		size_t sec_size() override;
		file * open_file() override;
		size_t size() override;
		bool is_deleted() override;
		file * create_file(char * name) override;
		void format();

	protected:
		int alloc_entry(char * name, byte flags, disk::sector_num first_sec, disk::sector_num * p_sector, size_t * p_offset);
		dos25 & fs;
		disk::sector_num first_sector;
		disk::sector_num end_sector;
		disk::sector_num sector;
		size_t           pos;		// position in sector
		int				 file_no;

		byte * buf;
	};

	filesystem::dir * root_dir() override;

protected:

	// DIR
	void dir_format();
	//byte * dir_buf;

	// VTOC
	
	inline void switch_sector_used(disk::sector_num sec) {
		vtoc_buf[10 + sec / 8] ^= (128 >> (sec & 7));
	}
	
	virtual void vtoc_init();
	virtual disk::sector_num alloc_sector();
	virtual void free_sector(disk::sector_num sector);

	virtual void vtoc_format();
	virtual void vtoc_read();
	virtual void vtoc_write();

	disk::sector_num vtoc_sec1;
	disk::sector_num vtoc_sec2;
	size_t vtoc_size;
	byte * vtoc_buf;
	bool vtoc_dirty;
};
