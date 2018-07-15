#include "dos_IIplus.h"

const size_t VTOC_BUF_SIZE = 256;


const size_t VTOC_DOS_VERSION = 0;
const size_t VTOC_CAPACITY = 1;
const size_t VTOC_FREE_SEC = 3;
const size_t VTOC_BITMAP = 10;

const disk::sector_num VTOC2_SECTOR = 359;
const disk::sector_num VTOC_SECTOR = 360;

std::string dos_IIplus::name()
{
	return "dosII+";
}

dos_IIplus::dos_IIplus(disk * d) : dos2(d)
{
}

bool dos_IIplus::enhanced_vtoc()
{
	return sector_count() > 944;
}

filesystem * dos_IIplus::format(disk * d)
{

	dos_IIplus * fs = new dos_IIplus(d);
	fs->vtoc_format();
	fs->dir_format();
	return fs;
}

void dos_IIplus::vtoc_init()
{
	vtoc_sec1 = 360;
	vtoc_sec2 = 0;
	
	vtoc_size = 90;
	if (enhanced_vtoc()) {
		vtoc_size = 128;
	}
}

void dos_IIplus::vtoc_write()
{
	write_sector(VTOC_SECTOR, vtoc_buf);
	if (enhanced_vtoc()) {
		write_sector(VTOC2_SECTOR, vtoc_buf + 128);
	}
	vtoc_dirty = 0;
}

void dos_IIplus::vtoc_read()
{
	read_sector(VTOC_SECTOR, vtoc_buf);
	if (enhanced_vtoc()) {
		read_sector(VTOC_SECTOR, vtoc_buf + 128);
	}
	vtoc_dirty = 0;
}

void dos_IIplus::vtoc_format()
{
	auto capacity = (d->sector_count() >= 707) ? 1009 : 707;
	memset(vtoc_buf, 0, VTOC_BUF_SIZE);

	vtoc_buf[VTOC_DOS_VERSION] = 3; // version?
	set_word(vtoc_buf, VTOC_CAPACITY, capacity);
	set_word(vtoc_buf, VTOC_FREE_SEC, capacity);

	for (size_t i = 0; i < vtoc_size; i++) vtoc_buf[VTOC_BITMAP + i] = 0xff;
	vtoc_buf[VTOC_BITMAP] = 0x0f;		// first 4 sectors are always preallocated
	switch_sector_used(VTOC_SECTOR);

	if (enhanced_vtoc()) {
		switch_sector_used(VTOC2_SECTOR);
	}

	for (auto i = dir_start; i <= dir_end; i++) {
		switch_sector_used(i);
	}
	vtoc_dirty = true;
	vtoc_write();
}

disk::sector_num dos_IIplus::alloc_sector()
/*
Purpose:
Find free sector on the disk and return it's number.
Mark the sector as used and decrement number of free sectors in the VTOC table.
*/
{
	disk::sector_num sec = 0;

	for (size_t i = 0; i < vtoc_size; i++) {
		auto b = vtoc_buf[VTOC_BITMAP + i];
		for (byte m = 128; m != 0; m /= 2) {
			if (b & m) {
				vtoc_buf[VTOC_BITMAP + i] = b ^ m;
				int free = read_word(vtoc_buf, VTOC_FREE_SEC);
				free--;
				set_word(vtoc_buf, VTOC_FREE_SEC, free);
				vtoc_dirty = true;
				vtoc_write();
				return sec;
			}
			sec++;
		}
	}
	throw "disk full";
}
