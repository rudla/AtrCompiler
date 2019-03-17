#include "xdos.h"

const size_t X_BOOT_FILE_LO = 0x032;
const size_t X_BOOT_FILE_HI = 0x034;

std::string xdos::name()
{
	return "xdos";
}

void xdos::set_dos_first_sector(disk::sector_num sector)
{
	d->write_word(1, X_BOOT_FILE_LO, X_BOOT_FILE_HI, word(sector));
}

disk::sector_num xdos::get_dos_first_sector()
{
	return read_word(1, X_BOOT_FILE_LO, X_BOOT_FILE_HI);
}

const filesystem::property * xdos::properties()
{
	static const filesystem::property props[] = {

		{ "BUFFERS", 1, 0x09, 1 },
		{ "RAMDISK", 1, 0x0B, 1 },
		{ "COLOR", 1, 0x10, 1 },
		{ nullptr, 0, 0, 0 }

	/*
	$070f: 01 02 03 04 05 06 07 00 (default)

	Changing these bytes you can exchange real drives with logical.
	For example D1 : could mean D3 : for real. 00 means RAM disk.
	*/
	};

	return props;
}

bool xdos::detect(disk * d)
{
	auto s = d->get_sector(1);
	return s->buf[0] == 0x58 && s->buf[X_BOOT_FILE_LO-1] == 0xA0 && s->buf[X_BOOT_FILE_HI-1] == 0xA2;
}

xdos::xdos(disk * d) : dos_IIplus(d)
{
}

filesystem * xdos::format(disk * d)
{
	auto fs = new xdos(d);
	fs->vtoc_format(1023, true);
	return fs;
}
