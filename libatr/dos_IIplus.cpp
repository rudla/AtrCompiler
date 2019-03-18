#include "dos_IIplus.h"

static const size_t yBOOT_FILE_LO = 0x03d;
static const size_t yBOOT_FILE_HI = 0x03f;

std::string dos_IIplus::name()
{
	return "II+";
}

bool dos_IIplus::detect(disk * d)
{
	auto s = d->get_sector(1);
	return s->buf[0] == 0xc4 && s->buf[yBOOT_FILE_LO - 1] == 0xA0 && s->buf[yBOOT_FILE_HI - 1] == 0xA9;
}

filesystem * dos_IIplus::format(disk * d)
{
	dos_IIplus * fs = new dos_IIplus(d);
	fs->vtoc_format(1023, true);
	return fs;
}

void dos_IIplus::set_dos_first_sector(disk::sector_num sector) 
{
	d->write_word(1, yBOOT_FILE_LO, yBOOT_FILE_HI, word(sector));
}

disk::sector_num dos_IIplus::get_dos_first_sector()
{
	return read_word(1, yBOOT_FILE_LO, yBOOT_FILE_HI);
}

const filesystem::property * dos_IIplus::properties()
{
	static const filesystem::property props[] = {

		{ "BUFFERS", 1, 0x09, 1 },
		{ "RAMDISK", 1, 0x0E, 1 },
		{ nullptr, 0, 0, 0 }

		/*
		$070f: 01 02 03 04 05 06 07 00 (default) 
		
		Changing these bytes you can exchange real drives with logical.
		For example D1 : could mean D3 : for real. 00 means RAM disk.
		*/
	};

	return props;
}

dos_IIplus::dos_IIplus(disk * d) : expanded_vtoc(d)
{
}
