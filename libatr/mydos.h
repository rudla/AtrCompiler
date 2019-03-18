/*
MyDOS


Format of MyDOS filesystem is simmilar to DOS 2.0.
For bigger densities, VTOC is handled differently.

## Single density

For single density, the format is compatible. 
MyDos however uses $2c4 (708) sectors.


## Subdirectories

Sectors  $169  through  $170 contain main disk directory data,
identifying  the  files  on  the disk, their sizes and their starting
sector number.

If the bit $10 is set in a directory entry, this entry is subdirectory.
Subdirectory has same format as main root directory of the disk. It's 8 sectors 
in a row.

## VTOC

Sector $168 (and sectors $167, $166, $165, etc.,
if  the  disk  is  formatted as a higher capacity disk not compatible
with  ATARI DOS 2) is used to hold a bit map of available sectors and
several  flag  byte  identifying  the  default format of files on the
disk. 

The default single sided format differs
only  in  that sector 720 is not left out of the file system in MYDOS
but  is  used to provide 708 free sectors on an empty diskette rather
than  707.  The  only  significant change made when the high capacity
format  is  chosen  are  that  enough  sectors before sector $168 are
allocated to assign a bit for each sector that may be allocated for a
file  or for use by the system (VTOC sectors). The high capacity disk
directory  may  be read by ATARI DOS 2, but the data in the files can
only  be  accessed  if it falls in the first 1023 sectors of the disk
and  then only if the file number checking code in DOS 2 is disabled.
This  format  allows MYDOS to support accessing disks of up to 65,535
sectors of 256 bytes each (approximately 16 Mbytes).

For medium density version in VTOC is 3, number of sectors is $403 (=1027).

NOTE: 256 bytes density disk has $23 as VTOC version?

## Boot sector

### $00 DOS version

MyDOS has 'M' here.

### $09 Buffer count

NUMBER OF FILES THAT MAY BE OPEN AT ONCE

Number of 128 bytes buffers (and open files).
MemLo depends on it!

### $0A RAM DISK UNIT #

### $0B DEFAULT UNIT NUMBER

### $0C,$0D ADDRESS OF THE FIRST BYTE OF FREE MEM.

OE sector size

### $10, $11 First sector of DOS.SYS program

SECTOR ADDRESS OF DOS.SYS



## Reference

[1] MYDOS Version 4 Technical User Guide, Revision 4.50; 1988; Charles Marslett, Robert Puff
[2] MYDOS Source code; 1984; Charles Marslett


*/

#pragma once

#include "expanded_vtoc.h"

class mydos : public expanded_vtoc
{
public:

	mydos(disk * d);

	std::string name();
	const property * properties();

	static filesystem * format(disk * d);
	static bool detect(disk * d);

	disk::sector_num get_dos_first_sector() override;
	void set_dos_first_sector(disk::sector_num sector) override;

	class mydos_dir : public dos2_dir {
	public:
		mydos_dir(mydos & fs, disk::sector_num sector);
		bool is_dir() override;
		dir * open_dir() override;
		dir * create_dir(char * name) override;
	};

	filesystem::dir * root_dir() override;

private:
	disk::sector_num alloc_dir();
};

