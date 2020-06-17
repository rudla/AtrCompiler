/*

Expanded VTOC is not complete filesystem.
This class implements VTOC management system used by multiple DOSes,
where basic VTC sector is same as in DOS 2 ($168* and additional VTOC 
sectors (if the disk is bigger) are allocated as sectors $167, $166, $165, etc.


## VTOC

Sector $168 (and sectors $167, $166, $165, etc.,
if  the  disk  is  formatted as a higher capacity disk not compatible
with  ATARI DOS 2) is used to hold a bit map of available sectors and
several  flag  byte  identifying  the  default format of files on the
disk.

The default single sided format differs
only  in  that sector 720 is not left out of the file system
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

*/

#pragma once

#include "dos2_filesystem.h"

class expanded_vtoc : public dos2 
{
protected:
	expanded_vtoc(disk * d, bool use_file_numbers = true,bool force_dos2_flag = false) : dos2(d, use_file_numbers,force_dos2_flag) {}

	void switch_sector_use(disk::sector_num sec, byte count = 1) override;
	disk::sector_num alloc_sector() override;
	void vtoc_format(disk::sector_num max_disk_size, bool reserve_720 = false, byte version = 3);

};
