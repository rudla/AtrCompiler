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

disk::disk(size_t sector_size, sector_num sector_count) : s_size(sector_size), s_count(sector_count)
{
	data = new byte[byte_size()];
	memset(data, 0, byte_size());
	for (int i = 0; i < cache_size; i++) {
		cache[i].init(*this);
		cache[i].next = i + 1;
		cache[i].prev = i - 1;
	}
	cache[0].prev = cache_size - 1;
	cache[cache_size - 1].next = 0;
	last = 0;
}

disk * disk::load(const std::string & filename)
{
	byte header[atr_header_size];

	ifstream f(filename, ios::binary);

	f.read((char*)header, atr_header_size);

	if (header[atr_magic] != 0x96 || header[atr_magic + 1] != 0x02) throw("This file is not an Atari disk file.");	
	size_t size = (peek_word(header, atr_disk_size_hi) << 16) + peek_word(header, atr_disk_size) * 16;
	size_t sector_size = peek_word(header, atr_sector_size);
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
	auto x = (s_count * s_size / 16) & 0xffff;
	poke_word(header, atr_disk_size, x);
	poke_word(header, atr_sector_size, s_size);

	f.write((char *)header, atr_header_size);

	flush();

	for (size_t i = 1; i <= 3; i++) {
		f.write((char *)sector_ptr(i), 128);
	}

	for (size_t i = 4; i <= s_count; i++) {
		if (i == 84) {
			char * s = (char *)sector_ptr(i);
			cout << "";
		}
		f.write((char *)sector_ptr(i), s_size);
	}
	f.close();
}

void disk::install_boot(const std::string & filename)
{
	ifstream f(filename, ios::binary);
	for (sector_num num = 1; num <= 3; num++) {
		auto s = get_sector(num);		
		f.read((char *)s->buf, 128);
		s->dirty = true;
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

disk::sector::sector()
{
	dirty = false;
	buf = nullptr;
}

void disk::sector::init(disk & d)
{
	num = 0;
	dirty = false;
	buf = new byte[d.sector_size()];
}

word disk::sector::dpeek(size_t offset) const
{
	return buf[offset] + buf[offset + 1] * 256;
}

void disk::sector::set(size_t offset, size_t size, byte b)
{
	memset(buf + offset, b, size);
	dirty = true;
}

void disk::sector::copy(size_t offset, const char * ptr, size_t size)
{
	memcpy(buf + offset, ptr, size);
	dirty = true;
}

disk::sector::~sector()
{
	delete[] buf;
}

void disk::write_sector(sector_num num, byte * data)
{
//	cout << "writing " << num << "\n";
//	if (num == 84) {
//		cout << "Here!\n";
//	}
	auto adr = sector_ptr(num);
	memcpy(sector_ptr(num), data, sector_size(num));
}

void disk::flush(sector & s)
{
	if (s.dirty) {
		write_sector(s.num, s.buf);
		s.dirty = false;
	}
}

void disk::flush()
{
	for (auto & s : cache) {
		flush(s);
	}
}

disk::sector * disk::get_sector(sector_num num, bool init)
{
	auto & recent = cache[last];

	if (recent.num == num) {
		return &recent;
	}

	for (int i = 0; i < cache_size; i++) {
		auto & s = cache[i];
		if (s.num == num) {
			cache[s.next].prev = s.prev;			
			cache[s.prev].next = s.next;
			if (i == last) last = cache[i].next;
			//last = s.next;
			s.next = last;
			s.prev = cache[last].prev;
			cache[last].prev = i;
			cache[s.prev].next = i;

			//print_cache();

			return &s;
		}
	}

	auto & s = cache[cache[last].next];
	flush(s);

	if (init) {
		memset(s.buf, 0, sector_size());
		s.dirty = true;
	} else {
		read_sector(num, s.buf);
	}
	s.num = num;
	last = cache[last].next;
	//print_cache();
	return &s;
}

disk::sector * disk::get_sector(sector_num num)
{
	return get_sector(num, false);
}

disk::sector * disk::init_sector(sector_num num)
{
	return get_sector(num, true);
}

void disk::print_cache()
{
	for (auto cnt = 0, i=last; cnt < cache_size; cnt++) {
		cout << cache[i].num << " ";
		i = cache[i].next;
	}
	cout << "\n";
}


byte disk::read_byte(sector_num sector, size_t offset)
{
	auto s = get_sector(sector);
	return s->peek(offset);
}

word disk::read_word(disk::sector_num sector, size_t offset)
{
	byte lo = read_byte(sector, offset);
	byte hi = read_byte(sector, offset+1);
	return size_t(hi) * 256 + lo;
}

word disk::read_word(disk::sector_num sector, size_t lo_offset, size_t hi_offset)
{
	byte lo = read_byte(sector, lo_offset);
	byte hi = read_byte(sector, hi_offset);
	return size_t(hi) * 256 + lo;
}

void disk::write_byte(sector_num sector, size_t offset, byte val)
{
	auto s = get_sector(sector);
	s->poke(offset, val);
}

void disk::write_word(sector_num sector, size_t offset, word val)
{
	auto s = get_sector(sector);
	s->dpoke(offset, val);
}

void disk::write_word(sector_num sector, size_t lo_offset, size_t hi_offset, word val)
{
	auto s = get_sector(sector);
	s->poke(lo_offset, val & 0xff);
	s->poke(hi_offset, (val >> 8) & 0xff);
}
