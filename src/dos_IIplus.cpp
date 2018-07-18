/*

First sector for DOS is specified by following code:

 073C: A0, 05    LDY #$05
 073E: A9, 00    LDA #$00
 0740: 20, 71, 07 JSR $0771

 To specify the boot start, we must therefore set LO,HI to $073D, $073f.

*/

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

const filesystem::property * dos_IIplus::properties()
{
	static const filesystem::property props[] = {
		{ "DOSSEC_LO",   1, 0x0739, 1 },
		{ "DOSSEC_HI",   1, 0x073f, 1 },

		{ "BUFFERS", 1, 0x09, 1 },
		/*
			=== $0709 Number of 128 bytes buffers (and open files).
			MemLo depends on it!
		*/

		{ "RAMDISK", 1, 0x0E, 1 },
		/* 
			=== $070E RAMdisk type 
			$8x -> 128KB , 1009 sectors in Medium Density 
			$2x -> 64KB (130XE), 499 sectors in Single Density 
			$4x -> 16KB (normal XL/XE) memory under ROM-OS 
			
			x -> 
			If it's 1, RAMdisk will be formated after DOS will load. 
			If it's a 0 RAMdisk will not be formated 
			and if it's 8, the RAMdisk will be write protected (very useful...)
		*/
		{ nullptr, 0, 0, 0 }

		/*
		$070f: 01 02 03 04 05 06 07 00 (default) 
		
		Changing these bytes you can exchange real drives with logical.
		For example D1 : could mean D3 : for real. 00 means RAM disk.
		*/
	};

	return props;
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
