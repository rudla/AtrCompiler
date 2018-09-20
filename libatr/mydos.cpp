#include "mydos.h"

#define DIR_FIRST_SECTOR 361

#define FLAG_DIRECTORY 0x10   // MyDOS

/*
const size_t VTOC_BUF_SIZE = 256;

const size_t VTOC_DOS_VERSION = 0;
const size_t VTOC_CAPACITY = 1;
*/
const size_t VTOC_FREE_SEC = 3;
/*
const size_t VTOC_BITMAP = 10;

const disk::sector_num VTOC2_SECTOR = 359;
const disk::sector_num VTOC_SECTOR = 360;
*/

std::string mydos::name()
{
	return "mydos";
}

static const filesystem::property dos_props[2] =
{
	{ "DOSSEC_LO",   1, 0x010, 1 },
	{ "DOSSEC_HI",   1, 0x011, 1 }
};

void mydos::set_dos_first_sector(disk::sector_num sector)
{
	set_property(&dos_props[0], sector & 0xff);
	set_property(&dos_props[0], (sector >> 8) & 0xff);
}

disk::sector_num mydos::get_dos_first_sector()
{
	auto lo = get_property_byte(&dos_props[0]);
	auto hi = get_property_byte(&dos_props[1]);
	return ((int)hi * 256) + lo;
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
	auto buf = new byte[d->sector_size()];
	d->read_sector(1, buf);
	bool a = buf[0] == 'M';
	delete[] buf;
	return a;
}

mydos::mydos(disk * d) : dos2(d)
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

mydos::mydos_dir::mydos_dir(dos2 & fs, disk::sector_num sector) : dos2_dir(fs, sector)
{

}

bool mydos::mydos_dir::is_dir()
{
	return (buf[pos] & FLAG_DIRECTORY) != 0;
}

filesystem::dir * mydos::mydos_dir::open_dir()
{
	return new mydos_dir(fs, peek_word(buf, pos + 3));
}

disk::sector_num mydos::free_sector_count()
{
	return peek_word(vtoc_buf, VTOC_FREE_SEC);;
}
