#include "dos2_filesystem.h"

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

using namespace std;

//===== DIR

const disk::sector_num DIR_FIRST_SECTOR = 361;
const size_t           DIR_SIZE = 8;

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


const size_t VTOC_BUF_SIZE = 128;

// ==== VTOC

const disk::sector_num VTOC_SECTOR = 0x168;

const size_t VTOC_VERSION = 0;		// = 0 id DOS 2.0
//const size_t VTOC_NUMBER_SECTORS_TOTAL = 0x2C3
const size_t VTOC_MAX_FREE_SEC = 1;	// = 0x2C3
const size_t VTOC_FREE_SEC     = 3;
const size_t VTOC_BITMAP       = 10;
const size_t VTOC_BITMAP_SIZE  = 90;  // number of bytes

//const size_t VTOC2_FREE_SEC = VTOC2_OFFSET + 122;

//==== BOOT
const size_t BOOT_FILE_LO = 0x0f;
const size_t BOOT_FILE_HI = 0x10;
const size_t BOOT_BUFFERS = 0x09;
const size_t BOOT_DRIVES  = 0x0A;  // bit for each drive $70A

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

	fs->dir_format();
	fs->vtoc_format();
	fs->vtoc_write();
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

dos2::dos2(disk * d) : filesystem(d)
{
	vtoc_buf = new byte[VTOC_BUF_SIZE];  // we need only 128 for single density disk
	vtoc_read();
}

dos2::~dos2()
{
	vtoc_write();
//	delete[] dir_buf;
	delete[] vtoc_buf;
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
	pos += 16;
	if (pos == fs.sector_size()) {
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
		sec = s->peek(sec_size - 2) + (s->peek(sec_size - 3) & 3 << 8);
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
	if (!writing) {
		sector = first_sec;		
	}
}

int dos2::dos2_dir::alloc_entry(char * name, byte flags, disk::sector_num first_sec, disk::sector_num * p_sector, size_t * p_offset)
{
	int file_no = 0;
	auto dir_buf = new byte[fs.sector_size()];
	
	for (auto sec = first_sector; sec < end_sector; sec++) {
		fs.read_sector(sec, dir_buf);
		for (size_t i = 0; i < fs.sector_size(); i += 16) {
			if (dir_buf[i] == 0 || (dir_buf[i] & FLAG_DELETED)) {

				// write name, size, etc.

				dir_buf[i] = flags;
				dir_buf[i + 1] = 0;
				dir_buf[i + 2] = 0;
				poke_word(dir_buf, i + 3, first_sec);
				//dir_buf[i + 3] = 0;
				//dir_buf[i + 4] = 0;
				memcpy(dir_buf + i + 5, name, 11);
				fs.write_sector(sec, dir_buf);
				delete[] dir_buf;
				*p_sector = sec;
				*p_offset = i;
				return file_no;
			}
			file_no++;
		}
	}
	delete[] dir_buf;
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
			write_sec(0);
		}

		auto dir = fs.get_sector(dir_sector);
		dir->poke(dir_pos, FLAG_IN_USE + FLAG_DOS2);
		dir->dpoke(dir_pos + DIR_FILE_SIZE, word(sec_cnt));
		dir->dpoke(dir_pos + DIR_FILE_START, word(first_sec));

		//fs.read_sector(dir_sector, buf);
		//buf[dir_pos] = FLAG_IN_USE;
		//buf[dir_pos] |= FLAG_DOS2;

		//poke_word(buf, dir_pos + 1, sec_cnt);
		//poke_word(buf, dir_pos + 3, first_sec);
		//fs.write_sector(dir_sector, buf);
	}
}

void dos2::dos2_file::write_sec(disk::sector_num next) 
{
	auto p = fs.sector_size();
	auto sec = fs.get_sector(sector);
	sec->poke(--p, byte(pos));
	sec->poke(--p, next & 0xff);
	sec->poke(--p, byte((file_no << 2) + (next >> 8)));

	//buf[--p] = byte(pos);
	//buf[--p] = next & 0xff;
	//buf[--p] = byte((file_no << 2) + (next >> 8));
	//fs.write_sector(sector, buf);
	
	sector = next;
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
	return fs.read_byte(sector, p - 2) + (fs.read_byte(sector, p - 3) & 3 << 8);
}

disk::sector_num dos2::dos2_file::first_sector()
{
	return first_sec;
}

bool dos2::dos2_file::next_sector()
{
	disk::sector_num next = sector_next();
	if (next == 0) return false;
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
	if (pos == fs.sector_size() - 3) {
		auto sec = fs.alloc_sector();
		if (first_sec == 0) {
			first_sec = sec;
			sector = sec;
			sec = fs.alloc_sector();
		}
		write_sec(sec);
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

const size_t boot_size = 3;

void dos2::vtoc_format()
{
	// Init VTOC

	auto capacity = 707;
	memset(vtoc_buf, 0, VTOC_BUF_SIZE);

	vtoc_buf[0] = 0;  // DOS_2.0	
	poke_word(vtoc_buf, VTOC_MAX_FREE_SEC, capacity);
	poke_word(vtoc_buf, VTOC_FREE_SEC, 707);

	for (size_t i = 0; i < VTOC_BITMAP_SIZE; i++) vtoc_buf[VTOC_BITMAP + i] = 0xff;
	vtoc_buf[VTOC_BITMAP] = 0x0f;		// first 4 sectors are always preallocated
	switch_sector_used(VTOC_SECTOR);
	for (auto sec = DIR_FIRST_SECTOR; sec < DIR_FIRST_SECTOR+DIR_SIZE; sec++) {
		switch_sector_used(sec);
	}
	vtoc_dirty = true;
}

void dos2::vtoc_write()
{
	if (vtoc_dirty) {
		write_sector(VTOC_SECTOR, vtoc_buf);
	}
	vtoc_dirty = false;
}

void dos2::vtoc_read()
{
	read_sector(VTOC_SECTOR, vtoc_buf);
	vtoc_dirty = false;
}

disk::sector_num dos2::alloc_sector()
/*
Purpose:
	Find free sector on the disk and return it's number.
	Mark the sector as used and decrement number of free sectors in the VTOC table.
*/
{
	disk::sector_num sec = 0;

	for (size_t i = 0; i < VTOC_BITMAP_SIZE; i++) {
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

void dos2::free_sector(disk::sector_num sector)
{
	switch_sector_used(sector);
	auto free = peek_word(vtoc_buf, VTOC_FREE_SEC);
	free--;
	poke_word(vtoc_buf, VTOC_FREE_SEC, free);
	vtoc_dirty = true;
}

disk::sector_num dos2::free_sector_count()
{
	disk::sector_num cnt = peek_word(vtoc_buf, VTOC_FREE_SEC);
	return cnt;
}

disk::sector_num dos2::get_dos_first_sector() 
{ 
	return read_word(1, BOOT_FILE_LO, BOOT_FILE_HI);
}

void dos2::set_dos_first_sector(disk::sector_num sector) 
{
	return write_word(1, BOOT_FILE_LO, sector);
}
