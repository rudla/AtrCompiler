/*

Header Structure

There is a 16 byte header with the following information (WORDs are in little endian format):

Identification <WORD>: ($9602). This word is the 16 bit sum of the individual ASCII values of the string of bytes: "NICKATARI".

Size of disk image <WORD>: The size is expressed in "paragraphs". A paragraph is sixteen bytes, thus Size = Image size in bytes / 16.

Sector size <WORD>: 128 ($80) or 256 ($100) bytes per sector. Note that the original documentation only specifies two sector sizes, in practice however there is also sector size 512 ($200). 512 byte sectors were introduced by SpartaDOS X, and are sometimes used to create large ATR images. Some Atari Emulators and Peripheral emulators such as Altirra and AspeQt can make use of the 512 byte sectors in ATR files.

High part of size <WORD>: in paragraphs (added by REV 3.00)

Disk flags <BYTE>: Flags such as copy protection and write protect. The 9th byte of the header contains information in individual bits. Bit 4 = 1 means the disk image is treated as copy protected (has bad sectors). Bit 5 = 1 means the disk is write protected.

1st bad sector <WORD>: The 10th and 11th bytes of the header is a word which contains the number of the first bad sector.

5 SPARE header bytes (contain zeroes)

After the header comes the disk image. This is just a continuous string of bytes, with the first 128 bytes being the contents of disk sector 1, the second being sector 2, etc. The first 3 sectors of an Atari disk must be 128 bytes long even if the rest of the sectors are 256 bytes or more.

Note that not all software will recognize or use the 1st bad sector header data, and some software do not follow the "first 3 sectors must be 128 bytes long" rule.
*/

#include "disk.h"
#include <iostream>
#include <fstream>

using namespace std;

enum atr_header
{
	atr_magic       = 0,  // WORD 0x96 0x02
	atr_disk_size   = 2,  // WORD (in multiples of 16)
	atr_sector_size = 4,  // WORD
	atr_disk_size_hi = 6,  // WORD
	atr_flags       = 8,  // BYTE
	atr_bad_sec     = 9,  // WORD
	atr_reserve     = 11,  // 5 bytes
	atr_header_size = 16			// size of the structure
};

disk * disk::load(const std::string & filename)
{
	byte header[atr_header_size];

	ifstream f(filename, ios::binary);

	f.read((char*)header, atr_header_size);

	if (header[atr_magic] != 0x96 || header[atr_magic + 1] != 0x02) throw("This file is not an Atari dosk file.");	
	size_t size = (read_word(header, atr_disk_size_hi) << 16) + read_word(header, atr_disk_size) * 16;
	size_t sector_size = read_word(header, atr_sector_size);
	size_t boot_sector_size = (sector_size >= 256 && (size & 0xff) == 0) ? sector_size : 128;

	sector_num sec_count = 3 + (size - 3 * boot_sector_size) / sector_size;

	disk * d = new disk(sector_size, sec_count);

	byte * sec = new byte[sector_size];

	for (size_t i = 1; i <= 3; i++) {
		f.read((char *)sec, boot_sector_size);
		d->write_sector(i, sec);
	}

	for (size_t i = 4; i <= sec_count; i++) {
		f.read((char *)sec, sector_size);
		d->write_sector(i, sec);
	}

	delete[] sec;

	return d;
}

void disk::save(const std::string & filename)
{
	ofstream f(filename, ios::binary);
	
	byte header[atr_header_size];
	memset(header, 0, atr_header_size);

	header[atr_magic] = 0x96;
	header[atr_magic + 1] = 0x02;
	auto x = (s_count / 16) & 0xffff;
	set_word(header, atr_disk_size, x);
	set_word(header, atr_sector_size, s_size);

	f.write((char *)header, atr_header_size);

	for (size_t i = 1; i <= 3; i++) {
		f.write((char *)sector_ptr(i), 128);
	}

	for (size_t i = 4; i <= s_count; i++) {
		f.write((char *)sector_ptr(i), s_size);
	}

}

void disk::install_boot(const std::string & filename)
{
	byte buf[128];
	ifstream f(filename, ios::binary);
	for (sector_num s = 1; s <= 3; s++) {
		f.read((char *)buf, 128);
		write_sector(s, buf);
	}
}

void disk::save_boot(const std::string & filename)
{
	byte buf[128];
	ofstream f(filename, ios::binary);
	for (sector_num s = 1; s <= 3; s++) {
		read_sector(s, buf);
		f.write((char *)buf, 128);
	}
}

disk::sector::sector(disk & d, sector_num num) : d(d), num(num), dirty(false)
{
	buf = new byte[d.sector_size()];
	d.read_sector(num, buf);
}

void disk::sector::write()
{
	if (dirty) {
		d.write_sector(num, buf);
		dirty = false;
	}
}

disk::sector::~sector()
{
	write();
	delete[] buf;
}

disk::sector * disk::get_sector(sector_num num)
{
	return new disk::sector(*this, num);
}
