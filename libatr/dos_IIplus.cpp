#include "dos_IIplus.h"

const size_t VTOC_BUF_SIZE = 256;


const size_t VTOC_DOS_VERSION = 0;
const size_t VTOC_CAPACITY = 1;
const size_t VTOC_FREE_SEC = 3;
const size_t VTOC_BITMAP = 10;

const disk::sector_num VTOC2_SECTOR = 359;
const disk::sector_num VTOC_SECTOR = 360;

#define DIR_FIRST_SECTOR 361
#define DIR_SIZE 8


const size_t BOOT_FILE_LO = 0x03d;
const size_t BOOT_FILE_HI = 0x03f;

std::string dos_IIplus::name()
{
	return "II+";
}

void dos_IIplus::set_dos_first_sector(disk::sector_num sector) 
{
	write_byte(1, BOOT_FILE_LO, sector & 0xff);
	write_byte(1, BOOT_FILE_HI, (sector >> 8) & 0xff);
}

disk::sector_num dos_IIplus::get_dos_first_sector()
{
	return read_word(1, BOOT_FILE_LO, BOOT_FILE_HI);
}

const filesystem::property * dos_IIplus::properties()
{
	static const filesystem::property props[] = {

		{ "BUFFERS", 1, 0x09, 1 },
		{ "RAMDISK", 1, 0x0E, 1 },
		{ nullptr, 0, 0, 0 }

		/*
		$070f: 01 02 03 04 05 06 07 00 (default) 
		
		Changing these bytes you can exchange real drives with logical.
		For example D1 : could mean D3 : for real. 00 means RAM disk.
		*/
	};

	return props;
}

bool dos_IIplus::detect(disk * d)
{
	auto buf = new byte[d->sector_size()];
	d->read_sector(1, buf);
	bool a = buf[0] == 0xc4 && buf[0x3c] == 0xA0 && buf[0x3e] == 0xA9;
	delete[] buf;
	return a;
}

dos_IIplus::dos_IIplus(disk * d) : dos25(d)
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
	poke_word(vtoc_buf, VTOC_CAPACITY, capacity);
	poke_word(vtoc_buf, VTOC_FREE_SEC, capacity);

	for (size_t i = 0; i < vtoc_size; i++) vtoc_buf[VTOC_BITMAP + i] = 0xff;
	vtoc_buf[VTOC_BITMAP] = 0x0f;		// first 4 sectors are always preallocated
	switch_sector_used(VTOC_SECTOR);

	if (enhanced_vtoc()) {
		switch_sector_used(VTOC2_SECTOR);
	}

	for (auto sec = DIR_FIRST_SECTOR; sec < DIR_FIRST_SECTOR + DIR_SIZE; sec++) {
		switch_sector_used(sec);
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
				int free = peek_word(vtoc_buf, VTOC_FREE_SEC);
				free--;
				poke_word(vtoc_buf, VTOC_FREE_SEC, free);
				vtoc_dirty = true;
				vtoc_write();
				return sec;
			}
			sec++;
		}
	}
	throw "disk full";
}

disk::sector_num dos_IIplus::free_sector_count()
{
	return peek_word(vtoc_buf, VTOC_FREE_SEC);;
}
