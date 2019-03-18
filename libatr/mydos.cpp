#include "mydos.h"

#define FLAG_DIRECTORY 0x10   // MyDOS
#define FLAG_IN_USE 0x40
#define FLAG_LOCKED 0x20

#define FLAG_MYDOS45 0x04

// LDY MAPBUF; WHICH TYPE DISK ?
// CPY #3; IF >2 THEN MYDOS 4.50 +
// BCC LLINKS
// ORA #$04


const size_t VTOC_FREE_SEC = 3;
const size_t VTOC_BITMAP = 10;


std::string mydos::name()
{
	return "mydos";
}

static const size_t BOOT_FILE_LO = 0x0f;
static const size_t BOOT_FILE_HI = 0x10;

void mydos::set_dos_first_sector(disk::sector_num sector)
{
	return write_word(1, BOOT_FILE_LO, word(sector));
}

disk::sector_num mydos::get_dos_first_sector()
{
	return read_word(1, BOOT_FILE_LO, BOOT_FILE_HI);
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

mydos::mydos(disk * d) : expanded_vtoc(d, false)
{
}

filesystem * mydos::format(disk * d)
{
	mydos * fs = new mydos(d);
	byte version = 3;
	if (d->sector_size() > 128) version = 0x23;
	fs->vtoc_format(0xffff, false, version);
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
	return (fs.read_byte(sector, pos) & FLAG_DIRECTORY) != 0;
}

filesystem::dir * mydos::mydos_dir::open_dir()
{
	return new mydos_dir(static_cast<mydos&>(fs), fs.read_word(sector, pos + 3));
}

disk::sector_num mydos::alloc_dir()
{
	// find 8 consecutive free sectors

	int vtoc_size = 128;
	disk::sector_num sec = 0, start = 0, vtoc_secno = VTOC_SECTOR;
	byte size = 0;
	auto vtoc = get_sector(vtoc_secno);

	for (size_t i = 0; i < vtoc_size; i++) {
		auto b = vtoc->buf[VTOC_BITMAP + i];
		for (byte m = 128; m != 0; m /= 2) {
			if (b & m) {
				if (size == 0) start = sec;
				size++;
				if (size == 8) {
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
	disk::sector_num sector;
	size_t offset;
	auto first_sector = static_cast<mydos&>(fs).alloc_dir();
	auto file_no = alloc_entry(name, FLAG_IN_USE | FLAG_DIRECTORY, first_sector, &sector, &offset);

	auto dir = new mydos_dir(static_cast<mydos&>(fs), first_sector);
	dir->format();
	return dir;
}
