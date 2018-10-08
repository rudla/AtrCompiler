#include "mydos.h"

#define DIR_FIRST_SECTOR 361

#define FLAG_DIRECTORY 0x10   // MyDOS
#define FLAG_IN_USE 0x40
#define FLAG_LOCKED 0x20

/*
const size_t VTOC_BUF_SIZE = 256;

const size_t VTOC_DOS_VERSION = 0;
const size_t VTOC_CAPACITY = 1;
*/
const size_t VTOC_FREE_SEC = 3;

const size_t VTOC_BITMAP = 10;

/*

const disk::sector_num VTOC2_SECTOR = 359;
const disk::sector_num VTOC_SECTOR = 360;
*/

std::string mydos::name()
{
	return "mydos";
}

const size_t BOOT_FILE_LO = 0x0f;
const size_t BOOT_FILE_HI = 0x10;

void mydos::set_dos_first_sector(disk::sector_num sector)
{
	return write_word(1, BOOT_FILE_LO, sector);
//	set_property(&dos_props[0], sector & 0xff);
//	set_property(&dos_props[0], (sector >> 8) & 0xff);
}

disk::sector_num mydos::get_dos_first_sector()
{
	return read_word(1, BOOT_FILE_LO, BOOT_FILE_HI);

//	auto lo = get_property_byte(&dos_props[0]);
//	auto hi = get_property_byte(&dos_props[1]);
//	return ((int)hi * 256) + lo;
}

const filesystem::property * mydos::properties()
{
	static const filesystem::property props[] = {

		{ "BUFFERS", 1, 0x09, 1 },
		{ "RAMDISK_UNIT", 1, 0x0A, 1 },
		{ "DEFAULT_UNIT", 1, 0x0B, 1 },
		{ nullptr, 0, 0, 0 }
	};

	return props;
}

bool mydos::detect(disk * d)
{
	auto s = d->get_sector(1);
	bool a = s->buf[0] == 'M';
	return a;
}

mydos::mydos(disk * d) : dos25(d)
{
}

filesystem * mydos::format(disk * d)
{
	mydos * fs = new mydos(d);
	fs->vtoc_format();
	fs->dir_format();
	return fs;
}

filesystem::dir * mydos::root_dir()
{
	return new mydos_dir(*this, DIR_FIRST_SECTOR);
}

mydos::mydos_dir::mydos_dir(mydos & fs, disk::sector_num sector) : dos2_dir(fs, sector)
{

}

bool mydos::mydos_dir::is_dir()
{
	return (buf[pos] & FLAG_DIRECTORY) != 0;
}

filesystem::dir * mydos::mydos_dir::open_dir()
{
	return new mydos_dir(static_cast<mydos&>(fs), peek_word(buf, pos + 3));
}

disk::sector_num mydos::free_sector_count()
{
	return peek_word(vtoc_buf, VTOC_FREE_SEC);;
}

disk::sector_num mydos::alloc_dir()
{
	// find 8 consecutive free sectors
	int vtoc_size = 128;
	disk::sector_num sec = 0, start = 0;
	byte size = 0;
	for (size_t i = 0; i < vtoc_size; i++) {
		auto b = vtoc_buf[VTOC_BITMAP + i];
		for (byte m = 128; m != 0; m /= 2) {
			if (b & m) {
				if (size == 0) start = sec;
				size++;
				if (size == 8) {
					for (int i = 0; i < size; i++) {
						switch_sector_used(start + i);
					}
					int free = peek_word(vtoc_buf, VTOC_FREE_SEC);
					free -= size;
					poke_word(vtoc_buf, VTOC_FREE_SEC, free);
					vtoc_dirty = true;
					return start;
				}
			} else {
				size = 0;
			}
			sec++;
		}
	}
	throw "disk full";

}

filesystem::dir * mydos::mydos_dir::create_dir(char * name)
{
	//auto & mfs = static_cast<mydos&>(fs);
	disk::sector_num sector;
	size_t offset;
	auto first_sector = static_cast<mydos&>(fs).alloc_dir();
	auto file_no = alloc_entry(name, FLAG_IN_USE | FLAG_DIRECTORY, first_sector, &sector, &offset);

	auto dir = new mydos_dir(static_cast<mydos&>(fs), first_sector);
	dir->format();
	return dir;
}
