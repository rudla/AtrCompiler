#include "sparta_dos.h"

// Directory flags

#define FLAG_DIR_END       0x00

#define FLAG_PROTECTED      0x01
#define FLAG_HIDDEN         0x02
#define FLAG_ARCHIVED       0x04
#define FLAG_IN_USE         0x08
#define FLAG_DELETED        0x10
#define FLAG_SUBDIRECTORY   0x20
#define FLAG_OPENED         0x80


// Offsets of data in sector 1

#define DIR_SECTOR          0x09  // 2 bytes
#define TOTAL_SECTOR_COUNT  0x0B  // 2 bytes
#define FREE_SECTOR_COUNT   0x0d  // 2 bytes
#define BITMAP_SECTOR_COUNT 0x0F  // 2 bytes
#define BITMAP_SECTOR       0x10  // 2 bytes
#define DATA_SECTOR_SEARCH  0x12  // 2 bytes
#define DIR_SECTOR_SEARCH   0x14  // 2 bytes
#define DISK_VOLUME_NAME    0x16  // 8 bytes
#define FS_VERSION          0x20  // 1 byte
#define VOLUME_SEQ_NUMBER   0x26  // 1 byte
#define VOLUME_RND_NUMBER   0x27  // 1 byte
#define AUTOEXEC_FILE_SECTOR  0x28 // 2 byte


using namespace std;

std::string sparta_dos::name()
{
	return "sparta";
}


const filesystem::property * sparta_dos::properties()
{
	static const filesystem::property props[2] = {
		{ "NAME",   1, DISK_VOLUME_NAME, 8 },
	{ nullptr, 0, 0, 0 }
	};

	return props;
}

sparta_dos::sparta_dos(disk * d) : filesystem(d)
{
	auto s = d->get_sector(1);
	dir_sector = peek_word(s->buf, DIR_SECTOR);
	free_count = peek_word(s->buf, FREE_SECTOR_COUNT);
}

sparta_dos::~sparta_dos()
{
	vtoc_write();
	delete[] dir_buf;
	delete[] vtoc_buf;
}

disk::sector_num sparta_dos::free_sector_count()
{
	return free_count;
}

static const filesystem::property dos_props[2] =
{
	{ "DOSSEC_LO",   1, AUTOEXEC_FILE_SECTOR, 1 },
	{ "DOSSEC_HI",   1, AUTOEXEC_FILE_SECTOR+1, 1 }
};

void sparta_dos::set_dos_first_sector(disk::sector_num sector)
{
	set_property(&dos_props[0], sector & 0xff);
	set_property(&dos_props[0], (sector >> 8) & 0xff);
}

disk::sector_num sparta_dos::get_dos_first_sector()
{
	auto lo = get_property_byte(&dos_props[0]);
	auto hi = get_property_byte(&dos_props[1]);
	return ((int)hi * 256) + lo;
}

bool sparta_dos::detect(disk * d)
{
	auto buf = new byte[d->sector_size()];

	d->read_sector(1, buf);

	bool is_sparta = buf[0x06] == 0x4C && (buf[0x07] == 0x80 && buf[0x08] == 0x30) || (buf[0x07] == 0x40 && buf[0x08] == 0x04);

	delete[] buf;
	return is_sparta;
}

filesystem * sparta_dos::format(disk * d)
{

	sparta_dos * fs = new sparta_dos(d);

	fs->dir_format();
	fs->vtoc_format();
	fs->vtoc_write();
	return fs;
}

void sparta_dos::dir_format()
{
	// Init DIR
	memset(dir_buf, 0, sector_size());
}

sparta_dos::sparta_dos_dir::sparta_dos_dir(sparta_dos_file * file) : f(file) {
	next();
	file->set_size(size());
	next();
}

sparta_dos::sparta_dos_dir::~sparta_dos_dir() {
	delete f;
}

filesystem::dir * sparta_dos::open_dir(disk::sector_num sector)
{
	auto file = new sparta_dos_file(*this, sector, 32000, false);
	return new sparta_dos_dir(file);
}

filesystem::dir * sparta_dos::root_dir()
{
	return open_dir(dir_sector);
}

bool sparta_dos::sparta_dos_dir::at_end()
{
	return f->eof() || buf[0] == FLAG_DIR_END;
}

void sparta_dos::sparta_dos_dir::next()
{
	((filesystem::file *)f)->read(buf, dir_entry_size);
}

filesystem::dir * sparta_dos::sparta_dos_dir::open_dir()
{
	return f->filesystem().open_dir(first_sector());
}

bool  sparta_dos::sparta_dos_dir::is_dir()
{
	return (buf[0] & FLAG_SUBDIRECTORY) != 0;
}

bool  sparta_dos::sparta_dos_dir::is_deleted()
{
	return (buf[0] & FLAG_DELETED) != 0;
}

std::string sparta_dos::sparta_dos_dir::name()
{
	char t[4 * 24];
	bool inverse = false;
	byte * filename = buf + dir_filename;

	size_t p = 0, non_space = 0;
	for (size_t i = 0; i < 11; i++) {
		if (i == 8 && !inverse) {
			p = non_space;
			t[p++] = '.';
		}
		byte b = filename[i];
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

size_t sparta_dos::sparta_dos_dir::size()
{
	return  peek_word(buf, dir_file_size) + 0x10000 * buf[dir_file_size + 2];
}

disk::sector_num sparta_dos::sparta_dos_dir::first_sector()
{
	return peek_word(buf, dir_first_sector);
}

size_t sparta_dos::sparta_dos_dir::sec_size()
{
	auto sec_size = f->filesystem().sector_size();
	auto sz = size();
	return sz;
	return (sz + (sec_size-1)) / sec_size;
}

filesystem::file * sparta_dos::sparta_dos_dir::open_file()
{
	return new sparta_dos_file(f->filesystem(), first_sector(), size(), false);
}

sparta_dos::sparta_dos_file::sparta_dos_file(sparta_dos & fs, disk::sector_num first_map, size_t size, bool writing) :
	fs(fs),
	first_map(first_map),
	writing(writing),
	byte_size(size)
{
	data_buf = new byte[fs.d->sector_size()];
	map_buf = new byte[fs.d->sector_size()];

	if (first_map) {
		fs.read_sector(first_map, map_buf);
		sector_map = first_map;
		map_offset = 2;
		sector = sector_next();
		if (sector) {
			fs.read_sector(sector, data_buf);
		}
	} else {
		sector = 0;
	}
	byte_pos = 0;
	pos = 0;
}


sparta_dos & sparta_dos::sparta_dos_file::filesystem()
{
	return fs;
}

size_t sparta_dos::sparta_dos_file::size()
{
	return 0;
}

void sparta_dos::sparta_dos_file::set_size(size_t size)
{
	byte_size = size;
}

sparta_dos::sparta_dos_file::~sparta_dos_file()
{
	delete[] map_buf;
	delete[] data_buf;
}

void sparta_dos::sparta_dos_file::write_sec(disk::sector_num next)
{
/*
	auto p = fs.sector_size();
	buf[--p] = byte(pos);
	buf[--p] = next & 0xff;
	buf[--p] = byte((file_no << 2) + (next >> 8));
	fs.write_sector(sector, buf);
	sector = next;
	sec_cnt++;
	pos = 0;
*/
}

bool sparta_dos::sparta_dos_file::sector_end()
{
	return pos == fs.sector_size();
}

disk::sector_num sparta_dos::sparta_dos_file::sector_next()
{
	map_offset += 2;
	if (map_offset == fs.sector_size()) {
		map_offset = 4;
		sector_map = peek_word(map_buf, 0);
		if (sector_map == 0) return 0;
	}

	disk::sector_num x = peek_word(map_buf, map_offset);
	return x;
}

disk::sector_num sparta_dos::sparta_dos_file::first_sector()
{
	return first_map;
}

bool sparta_dos::sparta_dos_file::eof()
{
	return byte_pos == byte_size;
}

byte sparta_dos::sparta_dos_file::read()
{
	if (pos == fs.sector_size()) {
		auto next = sector_next();
		if (!next) throw("EOF");
		sector = next;
		pos = 0;
		fs.read_sector(sector, data_buf);
	}

	if (byte_pos == byte_size) throw("EOF");

	byte_pos++;
	return data_buf[pos++];
}

void sparta_dos::sparta_dos_file::write(byte b)
{
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

const size_t boot_size = 3;


void sparta_dos::vtoc_format()
{
}

void sparta_dos::vtoc_write()
{
}

void sparta_dos::vtoc_read()
{
}

disk::sector_num sparta_dos::alloc_sector()
/*
Purpose:
	Find free sector on the disk and return it's number.
	Mark the sector as used and decrement number of free sectors in the VTOC table.
*/
{
	return 0;
}
