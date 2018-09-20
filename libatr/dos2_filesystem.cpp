#include "dos2_filesystem.h"

#define FLAG_NEVER_USED 0x00
#define FLAG_DELETED 0x80
#define FLAG_IN_USE 0x40
#define FLAG_LOCKED 0x20
#define FLAG_DOS2 0x02
#define FLAG_OPENED 0x01


using namespace std;

const size_t VTOC2_OFFSET = 16;
const size_t VTOC_BUF_SIZE = 256;

const size_t VTOC_MAX_FREE_SEC = 1;
const size_t VTOC_FREE_SEC     = 3;
const size_t VTOC_BITMAP       = 10;
const size_t VTOC2_FREE_SEC = VTOC2_OFFSET + 122;

const size_t BOOT_FILE_LO = 0x0f;
const size_t BOOT_FILE_HI = 0x10;

std::string dos2::name()
{
	return "2.5";
}


const filesystem::property * dos2::properties()
{
	static const filesystem::property props[3] = {
		{ "DRIVES",   1, 0x0a, 1},
		{ "BUFFERS",  1, 0x09, 1 },
	{ nullptr, 0, 0, 0}
	};

	return props;
}

dos2::dos2(disk * d) : filesystem(d)
{
	dir_buf = new byte[d->sector_size()];

	vtoc_init();
	vtoc_buf = new byte[VTOC_BUF_SIZE];  // we need only 128 for single density disk
	vtoc_read();
}

dos2::~dos2()
{
	vtoc_write();
	delete[] dir_buf;
	delete[] vtoc_buf;
}

bool dos2::detect(disk * d)
{
	return true; // this is default
}

filesystem * dos2::format(disk * d)
{

	dos2 * fs = new dos2(d);

	fs->dir_format();
	fs->vtoc_format();
	fs->vtoc_write();
	return fs;
}

disk::sector_num dos2::dir_start = 361;
disk::sector_num dos2::dir_size  = 8;

void dos2::dir_format()
{
	// Init DIR
	memset(dir_buf, 0, sector_size());
	for (auto sec = dir_start; sec < dir_start + dir_size; sec++) {
		write_sector(sec, dir_buf);
	}
}

dos2::dos2_dir::dos2_dir(dos2 & fs, disk::sector_num sector) : fs(fs), sector(sector) {
	end_sector = sector + dos2::dir_size;
	file_no = 1;
	pos = 0;
	buf = new byte[fs.sector_size()];
	fs.read_sector(sector, buf);
}

filesystem::dir * dos2::root_dir()
{
	return new dos2_dir(*this, dir_start);
}

bool dos2::dos2_dir::at_end()
{
	return sector >= end_sector;
}

bool dos2::dos2_dir::is_deleted()
{
	return (buf[pos] & FLAG_DELETED) != 0;
}

void dos2::dos2_dir::next()
{
	pos += 16;
	if (pos == fs.sector_size()) {
		sector++;
		if (at_end()) return;
		fs.read_sector(sector, buf);
		pos = 0;
	}
	file_no++;
	if (buf[pos] == 0) {
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
		byte b = buf[pos + i];
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
	return peek_word(buf, pos+1);
}

size_t dos2::dos2_dir::size()
{
	return sec_size() * 125;
}

filesystem::file * dos2::dos2_dir::open_file()
{
	return new dos2_file(fs, sector, pos, file_no, peek_word(buf, pos + 3), sec_size(), false);
}

dos2::dos2_file::dos2_file(dos2 & fs, disk::sector_num dir_sector, size_t dir_pos, int file_no, disk::sector_num first_sec, disk::sector_num sec_cnt, bool writing) :
	fs(fs), dir_sector(dir_sector), 
	file_no(file_no), 
	dir_pos(dir_pos), 
	first_sec(first_sec), 
	sec_cnt(sec_cnt), 
	writing(writing) 
{
	buf = new byte[fs.d->sector_size()];
	pos = 0;
	sector = 0;
	size = 0;
	created_by_dos2 = true;
	if (!writing) {
		sector = first_sec;
		fs.read_sector(sector, buf);
	}
}

filesystem::file * dos2::create_file(char * name)
{
	int file_no = 0;
	for (auto sec = dir_start; sec < dir_start+dir_size; sec++) {
		read_sector(sec, dir_buf);
		for (size_t i = 0; i < sector_size() ; i += 16) {
			if (dir_buf[i] == 0 || (dir_buf[i] & FLAG_DELETED)) {

				// write name, size, etc.

				dir_buf[i] = FLAG_IN_USE | FLAG_OPENED;
				dir_buf[i + 1] = 0;
				dir_buf[i + 2] = 0;
				dir_buf[i + 3] = 0;
				dir_buf[i + 4] = 0;
				memcpy(dir_buf + i + 5, name, 11);
				d->write_sector(sec, dir_buf);
				return new dos2_file(*this, sec, i, file_no, 0, 0, true);
			}
			file_no++;
		}
	}
	return nullptr;
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

		fs.read_sector(dir_sector, buf);
		buf[dir_pos] = FLAG_IN_USE;
		if (created_by_dos2) buf[dir_pos] |= FLAG_DOS2;

		poke_word(buf, dir_pos + 1, sec_cnt);
		poke_word(buf, dir_pos + 3, first_sec);
		fs.write_sector(dir_sector, buf);
	}
}

void dos2::dos2_file::write_sec(disk::sector_num next) 
{
	auto p = fs.sector_size();
	buf[--p] = byte(pos);
	buf[--p] = next & 0xff;
	buf[--p] = byte((file_no << 2) + (next >> 8));
	fs.write_sector(sector, buf);
	sector = next;
	sec_cnt++;
	pos = 0;
}

bool dos2::dos2_file::sector_end()
{
	return pos == buf[fs.sector_size() - 1];
}

disk::sector_num dos2::dos2_file::sector_next()
{
	auto p = fs.sector_size();
	return buf[p - 2] + ((buf[p - 3] & 3) << 8);
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
	fs.read_sector(sector, buf);
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

	return buf[pos++];
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

	buf[pos] = b;
	pos++;
	size++;
}

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

void dos2::vtoc_init()
{
	vtoc_sec1 = 360;
	if (d->sector_count() < 1024) {
		vtoc_sec2 = 0;
		vtoc_size = 90;
	} else {
		vtoc_sec2 = 1024;
		vtoc_size = 128;
	}
}

void dos2::vtoc_format()
{
	// Init VTOC

	auto capacity = (d->sector_count() >= 1024) ? 1010 : 707;
	memset(vtoc_buf, 0, VTOC_BUF_SIZE);

	vtoc_buf[0] = 2;  // DOS_2.0	
	poke_word(vtoc_buf, VTOC_MAX_FREE_SEC, capacity);
	poke_word(vtoc_buf, VTOC_FREE_SEC, 707);

	for (size_t i = 0; i < vtoc_size; i++) vtoc_buf[VTOC_BITMAP + i] = 0xff;
	vtoc_buf[VTOC_BITMAP] = 0x0f;		// first 4 sectors are always preallocated
	switch_sector_used(vtoc_sec1);
	if (vtoc_sec2) {
		//switch_sector_used(720);
		switch_sector_used(vtoc_sec2);
		poke_word(vtoc_buf, VTOC2_FREE_SEC, 303);
	} else {

	}
	for (auto sec = dir_start; sec < dir_start+dir_size; sec++) {
		switch_sector_used(sec);
	}
	vtoc_dirty = true;
}

void dos2::vtoc_write()
{
	if (vtoc_dirty) {
		write_sector(vtoc_sec1, vtoc_buf);
		if (vtoc_sec2) {
			write_sector(vtoc_sec2, vtoc_buf + VTOC2_OFFSET);
		}
	}
	vtoc_dirty = false;
}

void dos2::vtoc_read()
{
	read_sector(vtoc_sec1, vtoc_buf);
	if (vtoc_sec2) {
		read_sector(vtoc_sec2, vtoc_buf + VTOC2_OFFSET);
	}
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

	for (size_t i = 0; i < vtoc_size; i++) {
		auto b = vtoc_buf[VTOC_BITMAP + i];
		for (byte m = 128; m != 0; m /= 2) {
			if (b & m) {
				vtoc_buf[VTOC_BITMAP + i] = b ^ m;
				auto off = (sec < 720 || vtoc_sec2 == 0) ? VTOC_FREE_SEC : VTOC2_FREE_SEC;
				int free = peek_word(vtoc_buf, off);
				free--;
				poke_word(vtoc_buf, off, free);
				vtoc_dirty = true;
				vtoc_write();
				return sec;
			}
			sec++;
		}
	}
	throw "disk full";
}

disk::sector_num dos2::free_sector_count()
{
	disk::sector_num cnt = peek_word(vtoc_buf, VTOC_FREE_SEC);
	if (vtoc_sec2) {
		cnt += peek_word(vtoc_buf, VTOC2_FREE_SEC);
	}
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
