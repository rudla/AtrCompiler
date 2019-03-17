#pragma once

#include "filesystem.h"

/*
Sector 1:

	
*/


class rkdos : public filesystem
{
public:
	
	enum dir_entry {
		dir_entry_size = 0,				// 1 byte
		dir_flags = 1,					// 1 byte
		dir_cluster_start = 2,			// 2 bytes
		dir_cluster_size = 4,			// 1 byte
		dir_file_size = 5,				// 3 bytes
		dir_date = 8,		// 2 bytes
		dir_time = 10,		// 2 bytes
		dir_name = 12,
	};

	enum file_flags {
		file_dir = 1,
		file_protected = 2,
		file_deleted = 4
	};

	enum root {
		root_id1 = 6,		  // 'R'
		root_id2 = 7,		  // 'K'
		root_first_free = 8,  // 8,9
		root_free_size = 10,  // 10,11
		root_entry = 12,
		root_entry_end = root_entry + dir_name		// we have no name here
	};

	static disk::sector_num root_sec; // = 1

	class rkdos_file : public filesystem::file
	{
	public:

		rkdos_file(rkdos & fs, rkdos_file * dir, word dir_pos, disk::sector_num cluster_start, byte cluster_size, size_t file_size, bool writing);
		~rkdos_file();

		bool eof() override;
		byte read() override;
		void write(byte b) override;
		disk::sector_num first_sector() override;

		rkdos & filesystem();

		size_t size();

		void seek(size_t pos);
		void seek_end();

	private:
		size_t sector_size();


		rkdos & fs;
		rkdos_file * dir;
		word         dir_pos;

		disk::sector dir_sector;			// position in directory (used when writing)
		word         dir_offset;

		disk::sector_num first_cluster;		// first cluster
		word             first_cluster_size;

		disk::sector_num cluster_end;
		word cluster_end_offset;

		disk::sector_num sector;			// current position in file
		word offset;
		size_t  file_pos;

		size_t file_size;

	};

	class rkdos_dir : public filesystem::dir
	{
	public:
		rkdos_dir(rkdos_file * file);
		~rkdos_dir();

		void next() override;
		bool at_end() override;
		std::string name() override;
		size_t sec_size() override;
		file * open_file() override;
		size_t size() override;
		bool is_dir() override;
		bool is_deleted() override;
		dir * open_dir() override;
		file * create_file(char * name) override;

	private:
		disk::sector_num cluster_start();
		byte cluster_size();

		rkdos_file * file;
		word  pos;
		byte entry_size;
		byte buf[40];
		bool end;
	};

	rkdos(disk * d);
	virtual ~rkdos();

	std::string name() override;
	const property * properties();
	filesystem::dir * root_dir() override;

	disk::sector_num free_sector_count() override;
	disk::sector_num get_dos_first_sector() override;
	void set_dos_first_sector(disk::sector_num sector) override;

	static filesystem * format(disk * d);
	static bool detect(disk * d);

private:
	// Free cluster
	disk::sector_num free_start;
	word free_size;

	disk::sector_num alloc_sector();
};
