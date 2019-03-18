#include "expanded_vtoc.h"

disk::sector_num expanded_vtoc::alloc_sector()
{
	disk::sector_num num = 0;
	disk::sector_num vtoc_sec = VTOC_SECTOR;
	size_t sector_offset;
	size_t offset = VTOC_BITMAP;
	size_t byte_count;

	do {
		byte_count = sector_size() - offset;

		if (find_free_sector(vtoc_sec, offset, byte_count, &sector_offset)) {
			num += sector_offset;
			switch_sector_use(num);
			return num;
		}
		sector_offset = 0;
		num += byte_count * 8;
		vtoc_sec--;
	} while (num < sector_count());


	throw "disk full";
}


void expanded_vtoc::switch_sector_use(disk::sector_num sec, byte count)
{
	do {

		disk::sector_num num = sec;
		disk::sector_num vtoc_sec = VTOC_SECTOR;
		size_t vtoc_bitmap = VTOC_BITMAP;

		do {
			size_t sec_cnt = (sector_size() - vtoc_bitmap) * 8;
			if (num <= sec_cnt) break;
			vtoc_sec--;
			vtoc_bitmap = 0;
			num -= sec_cnt;
		} while (true);

		auto vtoc = get_sector(vtoc_sec);

		auto off = vtoc_bitmap + num / 8;
		auto bit = 128 >> (num & 7);
		auto n = vtoc->peek(off);
		n ^= bit;
		vtoc->poke(off, n);

		vtoc = get_sector(VTOC_SECTOR);
		int free = vtoc->dpeek(VTOC_FREE_SEC);
		if (n & bit) {
			free++;
		} else {
			free--;
		}
		vtoc->dpoke(VTOC_FREE_SEC, free);

		sec++;
	} while (--count > 0);
}

void expanded_vtoc::vtoc_format(disk::sector_num max_disk_size, bool reserve_720, byte version)
/*
	DOS 2.0 does not use sector 720. Some DOSes follow this covention. If they do, reserve_720 is set to true.
*/
{

	auto disk_size = d->sector_count();
	auto sec_num = VTOC_SECTOR + 1;
	if (disk_size > max_disk_size) disk_size = max_disk_size;

	auto size = word(disk_size);
	size -= 3;		// first three sectors are used by boot
	byte vtoc_size = 0;
	// 00000000 10000000 11000000 11100000 11110000 11111000 11111100 11111110
	byte rest[8] = { 0x00,    0x80,    0xC0,    0xE0,    0xF0,    0xF8,    0xFC,    0xFE };

	do {
		sec_num--;
		auto s = d->init_sector(sec_num);
		word head = 0;

		if (vtoc_size == 0) {
			byte vers = (disk_size > 720) ? version : 2;
			s->poke(VTOC_VERSION, vers);
			s->dpoke(VTOC_CAPACITY, size);
			s->dpoke(VTOC_FREE_SEC, size);
			s->poke(VTOC_BITMAP, 0x0f);		// first 4 sectors are used by boot	
			head = VTOC_BITMAP + 1;
			size -= 4;						// we manage first 4 sectors 'by hand'
		}

		auto x = word((sector_size() - head) * 8);
		if (x > size) {
			x = size;
		}

		size_t bytes = x / 8;
		s->set(head, bytes, 0xff);

		if (auto r = x % 8) {
			s->poke(head + bytes, rest[r]);
		}
		vtoc_size++;
		size -= x;

	} while (size > 0);

	// Mark the sectors used by VTOC as used
	switch_sector_use(sec_num, vtoc_size);

	dir_format();

	if (reserve_720) {
		switch_sector_use(720);
	}

	// Free sectors when the disk is freshly initialized is stored as disk capacity

	size = read_word(VTOC_SECTOR, VTOC_FREE_SEC);
	write_word(VTOC_SECTOR, VTOC_CAPACITY, size);

	//	switch_sector_use(1039);

}
