/*
Volume table of contents(VTOC)

Sector 360 is the VTOC. It has this structure:

Byte 0 : Directory type(always 0)
Byte 1 - 2 : Maximum sector number (always 02C5, which is incorrect given that this equals 709 and the actual maximum sector number used by the FMS is 719. 
             Due to this error, this value is ignored.)
Byte 3 - 4 : Number of sectors available (starts at 709 and changes as sectors are allocated or deallocated)
Byte 10 - 99 : Bitmap showing allocation of sectors.The high - order bit of byte 10 corresponds to(the nonexistent) sector 0 (so it is unused), 
               the next - lower bit corresponds to sector 1, and so on through sector 7 for the low - order bit of byte 10, 
			   then sector 8 for the high - order bit of byte 11, and so on.These are set to 1 if the sector is available, and 0 if it is in use.
*/

#pragma once

#include "filesystem.h"


class dos2 : public filesystem
{
public:


	// ==== VTOC

	static disk::sector_num VTOC_SECTOR;

	enum {
		VTOC_VERSION = 0,		// = 0 id DOS 2.0
		VTOC_CAPACITY = 1,		// = 0x2C3
		VTOC_FREE_SEC = 3,
		VTOC_BITMAP = 10,
		VTOC_BITMAP_SIZE = 90  // number of bytes
	};

	static disk::sector_num DIR_FIRST_SECTOR;
	static size_t           DIR_SIZE;

	dos2(disk * d, bool use_file_number = true, bool force_dos2_flag=false);
	~dos2();

	static filesystem * format(disk * d);
	static bool detect(disk * d);

	std::string name() override;
	const property * properties() override;


	disk::sector_num free_sector_count() override;

	disk::sector_num get_dos_first_sector() override;
	void set_dos_first_sector(disk::sector_num sector) override;

	filesystem::dir * root_dir() override;

	class dos2_file : public filesystem::file
	{
	public:

		dos2_file(dos2 & fs, disk::sector_num dir_sector, size_t dir_pos, int file_no, disk::sector_num first_sec, disk::sector_num sec_cnt, bool writing);
		~dos2_file();

		bool eof() override;
		byte read() override;
		void write(byte b) override;
		disk::sector_num first_sector() override;

	protected:

		void write_data_sector(disk::sector_num next);
		bool sector_end();
		disk::sector_num sector_next();
		bool next_sector();

		dos2 & fs;

		disk::sector_num first_sec;

		// current position
		disk::sector_num sector;
		size_t pos;

		bool writing;

		// dir
		disk::sector_num dir_sector;
		size_t           dir_pos;		// position in sector
		int				 file_no;
		
		size_t sec_cnt;					// size if not known yet
		size_t size;					// size in bytes
		bool dos2_compatible;
                
	};

	class dos2_dir : public filesystem::dir
	{
	public:
		dos2_dir(dos2 & fs, disk::sector_num sector);
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
		int alloc_entry(const char * name, byte flags, disk::sector_num first_sec, disk::sector_num * p_sector, size_t * p_offset);
		dos2 & fs;
		disk::sector_num first_sector;
		disk::sector_num end_sector;
		disk::sector_num sector;	// current sector
		size_t           pos;		// position in current sector
		int				 file_no;
	};

protected:

	// DIR
	void dir_format();

	// VTOC

	bool find_free_sector(disk::sector_num vtoc_sec, size_t offset, size_t byte_count, disk::sector_num * p_sec);

	virtual void switch_sector_use(disk::sector_num sec, byte count = 1);

	virtual disk::sector_num alloc_sector();
	virtual void free_sector(disk::sector_num sector);

	virtual void vtoc_format(byte version);

	bool use_file_number;
	byte fs_file_flags;
    bool force_dos2_flag;
};
