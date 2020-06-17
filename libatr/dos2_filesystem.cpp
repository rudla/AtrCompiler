#include "dos2_filesystem.h"
#include <cassert>

using namespace std;

//===== DIR

disk::sector_num dos2::DIR_FIRST_SECTOR = 361;
size_t           dos2::DIR_SIZE = 8;
size_t           dos2::DIR_ENTRY_SIZE = 16;
size_t           dos2::DIR_ENTRIES_PER_SECTOR = 8;

enum DirFlags {
	FLAG_NEVER_USED = 0x00,
	FLAG_OPENED = 0x01,
	FLAG_DOS2 = 0x02,
	FLAG_LOCKED = 0x20,
	FLAG_IN_USE = 0x40,
	FLAG_DELETED = 0x80
};

const size_t DIR_FILE_SIZE = 1;
const size_t DIR_FILE_START = 3;
const size_t DIR_FILE_NAME = 5;


// ==== VTOC

disk::sector_num dos2::VTOC_SECTOR = 0x168;

//==== BOOT
static const size_t BOOT_FILE_LO = 0x0f;
static const size_t BOOT_FILE_HI = 0x10;
static const size_t BOOT_BUFFERS = 0x09;
static const size_t BOOT_DRIVES  = 0x0A;  // bit for each drive $70A

std::string dos2::name()
{
	return "2";
}

bool dos2::detect(disk * d)
{
	if (d->sector_count() <= 720) {
		if (d->read_byte(1, 0) == 0 && d->read_byte(VTOC_SECTOR, VTOC_VERSION) == 2) return true;
	}
	return false;
}

filesystem * dos2::format(disk * d)
{

	dos2 * fs = new dos2(d);
	fs->vtoc_format(2);
	fs->dir_format();
	return fs;
}


const filesystem::property * dos2::properties()
{
	static const filesystem::property props[3] = {
		{ "DRIVES",   1, BOOT_DRIVES, 1},
		{ "BUFFERS",  1, BOOT_BUFFERS, 1 },
	{ nullptr, 0, 0, 0}
	};

	return props;
}

dos2::dos2(disk * d, bool use_file_number) : filesystem(d), use_file_number(use_file_number)
{
}

dos2::~dos2()
{
}

void dos2::dir_format()
{
	auto root = root_dir();
	static_cast<dos2::dos2_dir *>(root)->format();
	delete root;
}

void dos2::dos2_dir::format()
{
	for (auto sec = first_sector; sec < end_sector; sec++) {
		auto s = fs.get_disk()->init_sector(sec);
		fs.switch_sector_use(sec);
	}
}

filesystem::dir * dos2::root_dir()
{
	return new dos2_dir(*this, DIR_FIRST_SECTOR);
}

dos2::dos2_dir::dos2_dir(dos2 & fs, disk::sector_num first_sector) : fs(fs), first_sector(first_sector) 
{
	end_sector = first_sector + DIR_SIZE;
	sector = first_sector;
	file_no = 0;
	pos = 0;
}

bool dos2::dos2_dir::at_end()
{
	return sector >= end_sector;
}

bool dos2::dos2_dir::is_deleted()
{
	return (fs.read_byte(sector, pos) & FLAG_DELETED) != 0;
}

void dos2::dos2_dir::next()
{
	pos += DIR_ENTRY_SIZE;
	if (pos == DIR_ENTRIES_PER_SECTOR * DIR_ENTRY_SIZE) {		// fs.sector_size()
		sector++;
		if (at_end()) return;
		pos = 0;
	}
	file_no++;
	if (fs.read_byte(sector, pos) == 0) {
		sector = end_sector;
	}
}

std::string dos2::dos2_dir::name()
{
	char t[4*24];
	bool inverse = false;
	size_t p = 0, non_space = 0;
	for (size_t i = 5; i < 16; i++) {
		if (i == 5 + 8 && !inverse) {
			p = non_space;
			t[p++] = '.';
		}
		byte b = fs.read_byte(sector, pos + i);
		if (b >= 128) {
			if (!inverse) {
				inverse = true;
				t[p++] = '\\'; t[p++] = 'i';
			}
			b = b & 0x7f;
		} else {
			if (inverse) {
				t[p++] = '\\'; t[p++] = 'i';
				inverse = false;
			}
		}
		if (b < 32 || b == 123 || b >= 125) {
			const char * hex_digit = "0123456789ABCDEF";
			t[p++] = '\\'; t[p++] = 'x'; t[p++] = hex_digit[b >> 4]; t[p++] = hex_digit[b & 0xf];
		} else {
			if (b == ' ' || b == '\\') t[p++] = '\\';
			char c = char(b);
			t[p++] = c;
		}
		if (b != ' ' || inverse) non_space = p;
	}
	t[non_space] = 0;
	return string(t);
}

size_t dos2::dos2_dir::sec_size()
{
	return fs.read_word(sector, pos + DIR_FILE_SIZE);
}

size_t dos2::dos2_dir::size()
{
	size_t size = 0;
	auto sec_size = fs.sector_size();

	disk::sector_num sec = fs.read_word(sector, pos + DIR_FILE_START);
	while (sec) {
		auto s = fs.get_sector(sec);
		size += s->peek(sec_size - 1);
		sec = s->peek(sec_size - 2) + ((s->peek(sec_size - 3) & 3) << 8);
	}
	return size;
}

filesystem::file * dos2::dos2_dir::open_file()
{
	return new dos2_file(fs, sector, pos, file_no, fs.read_word(sector, pos + DIR_FILE_START), sec_size(), false);
}

dos2::dos2_file::dos2_file(dos2 & fs, disk::sector_num dir_sector, size_t dir_pos, int file_no, disk::sector_num first_sec, disk::sector_num sec_cnt, bool writing) :
	fs(fs), dir_sector(dir_sector), 
	file_no(file_no), 
	dir_pos(dir_pos), 
	first_sec(first_sec), 
	sec_cnt(sec_cnt), 
	writing(writing) 
{
	pos = 0;
	sector = 0;
	size = 0;
	dos2_compatible = true;
	if (!writing) {
		sector = first_sec;		
	}
}

int dos2::dos2_dir::alloc_entry(const char * name, byte flags, disk::sector_num first_sec, disk::sector_num * p_sector, size_t * p_offset)
{
	int file_no = 0;
	
	for (auto sec = first_sector; sec < end_sector; sec++) {
		auto s = fs.get_sector(sec);
		for (size_t i = 0; i < DIR_ENTRIES_PER_SECTOR * DIR_ENTRY_SIZE; i += DIR_ENTRY_SIZE) {
			auto f = s->peek(i);
			if (f == 0 || (f & FLAG_DELETED)) {
				s->poke(i, flags);
				s->dpoke(i + DIR_FILE_SIZE, 0);
				s->dpoke(i + DIR_FILE_START, word(first_sec));
				s->copy(i + DIR_FILE_NAME, name, 11);
				*p_sector = sec;
				*p_offset = i;
				return file_no;
			}
			file_no++;
		}
	}
	throw "dir full";
}

filesystem::file * dos2::dos2_dir::create_file(char * name)
{
	disk::sector_num sector;
	size_t offset;
	auto file_no = alloc_entry(name, FLAG_IN_USE | FLAG_OPENED, 0, &sector, &offset);
	return new dos2_file(fs, sector, offset, file_no, 0, 0, true);
}

dos2::dos2_file::~dos2_file()
{
	if (writing) {
		if (pos > 0) {
			if (first_sec == 0) {
				auto sec = fs.alloc_sector();
				first_sec = sec;
				sector = sec;
			}
			write_data_sector(0);
		}

		auto dir = fs.get_sector(dir_sector);
		dir->poke(dir_pos, FLAG_IN_USE + (dos2_compatible ? FLAG_DOS2: 0) + (fs.use_file_number ? 0 : 0x04));
		dir->dpoke(dir_pos + DIR_FILE_SIZE, word(sec_cnt));
		dir->dpoke(dir_pos + DIR_FILE_START, word(first_sec));
	}
}

void dos2::dos2_file::write_data_sector(disk::sector_num next) 
{
	auto p = fs.sector_size();
	auto sec = fs.get_sector(sector);
	byte sec_lo = next & 0xff;
	byte sec_hi = byte(next >> 8);
	
	if (fs.use_file_number) {
		assert(sec_hi <= 3);
		sec_hi |= file_no << 2;
	}
	sec->poke(--p, byte(pos));
	sec->poke(--p, sec_lo);
	sec->poke(--p, sec_hi);
	
	sector = next;
	if (sector > 720) {
		dos2_compatible = false;
	}
	sec_cnt++;
	pos = 0;
}

bool dos2::dos2_file::sector_end()
{
	return pos == fs.read_byte(sector, fs.sector_size() - 1);
}

disk::sector_num dos2::dos2_file::sector_next()
{
	auto p = fs.sector_size();
	byte sec_lo = fs.read_byte(sector, p - 2);
	byte sec_hi = fs.read_byte(sector, p - 3);

	if (fs.use_file_number) sec_hi &= 3;
	
	return  sec_lo + (sec_hi << 8);
}

disk::sector_num dos2::dos2_file::first_sector()
{
	return first_sec;
}

bool dos2::dos2_file::next_sector()
{
	disk::sector_num next = sector_next();
	if (next == 0 || next > fs.sector_count()) return false;
	sector = next;
	pos = 0;	
	return true;
}

bool dos2::dos2_file::eof()
{
	do {
		if (!sector_end()) return false;
	} while (next_sector());	
	return true;
}

byte dos2::dos2_file::read()
{
	while (sector_end()) {
		if (!next_sector())
			throw("EOF");
	}

	return fs.read_byte(sector, pos++);
}

void dos2::dos2_file::write(byte b) 
{
	if (first_sec == 0) {
		first_sec = fs.alloc_sector();
		sector = first_sec;
	}
	if (pos == fs.sector_size() - 3) {
		auto sec = fs.alloc_sector();
		write_data_sector(sec);
	}

	fs.write_byte(sector, pos, b);
	pos++;
	size++;
}



/*
VTOC buffer layout
==================

VTOC1 and VTOC2 sector are overlaid.

VTOC1                                     VTOC2
-------------------------------------     --------------------------
       0: dir type
    1..2: max sec number
    3..4: free sector count
  10..16: sector bitmap (sector 0..47)   
  17..99: sector bitmap (sector 48..719)     0..83: Repeat VTOC bitmap for sectors 48..719
100..127: unused (in vtoc1)                84..112: Bitmap for sectors 720..951
128..137:                                 112..121: Bitmap for sectors 952..1023
138..139: 								  122..123: Current number of free sectors above sector 719.
140..143:								  124..127: Unused.

*/

void dos2::vtoc_format(byte version)
{
	// Init VTOC
	// 1. first 3 sectors are used for boot
	// 2. 8 sectors are used for dir
	// 3. vtoc sector is used
	// 4. 720 sector is unused


	auto vtoc = d->init_sector(VTOC_SECTOR);

	vtoc->poke(VTOC_VERSION, version);							// DOS_2.0	
	vtoc->dpoke(VTOC_CAPACITY, 707);
	vtoc->dpoke(VTOC_FREE_SEC, 707+8+1);					// 719 - (3(boot) + 1(vtoc) + 8(dir))
	vtoc->set(VTOC_BITMAP, VTOC_BITMAP_SIZE, 0xff);
	
	vtoc->poke(VTOC_BITMAP, 0x0f);							// first 4 sectors are used by boot	
	switch_sector_use(VTOC_SECTOR);
	//for (auto sec = DIR_FIRST_SECTOR; sec < DIR_FIRST_SECTOR+DIR_SIZE; sec++) {
	//	switch_sector_use(sec);
	//}
}

void dos2::switch_sector_use(disk::sector_num sec, byte count) 
{
	do {
		auto vtoc = get_sector(VTOC_SECTOR);
		auto off = VTOC_BITMAP + sec / 8;
		auto bit = 128 >> (sec & 7);
		auto n = vtoc->peek(off);
		n ^= bit;
		vtoc->poke(off, n);

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

bool dos2::find_free_sector(disk::sector_num vtoc_sec, size_t offset, size_t byte_count, disk::sector_num * p_sec)
{
	auto vtoc = get_sector(vtoc_sec);

	for (size_t i = 0; i < byte_count; i++) {		
		if (auto b = vtoc->peek(offset + i)) {
			byte sec = 0;
			for (byte bit = 128; (b & bit) == 0; bit /= 2) {
				sec++;
			}
			*p_sec = i * 8 + sec;
			return true;

		}
	}
	return false;
}

disk::sector_num dos2::alloc_sector()
/*
Purpose:
	Find free sector on the disk and return it's number.
	Mark the sector as used and decrement number of free sectors in the VTOC table.
*/
{
	disk::sector_num sec = 0;
	if (find_free_sector(VTOC_SECTOR, VTOC_BITMAP, VTOC_BITMAP_SIZE, &sec)) {
		switch_sector_use(sec);
		return sec;
	}
	throw "disk full";
}

void dos2::free_sector(disk::sector_num sector)
{
	switch_sector_use(sector);
}

disk::sector_num dos2::free_sector_count()
{
	return read_word(VTOC_SECTOR, VTOC_FREE_SEC);
}

disk::sector_num dos2::get_dos_first_sector() 
{ 
	return read_word(1, BOOT_FILE_LO, BOOT_FILE_HI);
}

void dos2::set_dos_first_sector(disk::sector_num sector) 
{
	return write_word(1, BOOT_FILE_LO, word(sector));
}
