/*
DOS II+


Format of DOS II+ filesystem is simmilar to DOS 2.0.
For bigger densities, VTOC is handled differently.

## Single density

For single density, the format is compatible.
VTOC version is 02.
Standard number of $2c3 (707) sectors sre used.

## VTOC

VTOC is in sectors 359, 360

sector 360    

0        disk version 3 - medium density, 2 - signle density (do it is compatible with dos 2)
1..2     disk capacity = $31f (1009)
3..4     number of free sectors (1009 at the start)
5..10    unused
11..127  

sector 359    

0..9  additional 10 bytes of VTOC

## Boot sector

### $00 DOS version

DOS II+ version is $C4.

### $09 Buffer count

Number of 128 bytes buffers (and open files).
MemLo depends on it!

### $0E RAMdisk type
$8x -> 128KB , 1009 sectors in Medium Density
$2x -> 64KB (130XE), 499 sectors in Single Density
$4x -> 16KB (normal XL/XE) memory under ROM-OS

x ->
If it's 1, RAMdisk will be formated after DOS will load.
If it's a 0 RAMdisk will not be formated
and if it's 8, the RAMdisk will be write protected (very useful...)


First sector for DOS is specified by following code:

073C: A0, 05    LDY #$05
073E: A9, 00    LDA #$00
0740: 20, 71, 07 JSR $0771

To specify the boot start, we must therefore set LO,HI to $073D, $073f. (Byte $3D and $3F on sector 1.)


*/


#pragma once

#include "expanded_vtoc.h"

class dos_IIplus : public expanded_vtoc
{
public:
	dos_IIplus(disk * d);

	std::string name();
	const property * properties();

	static filesystem * format(disk * d);
	static bool detect(disk * d);

	disk::sector_num get_dos_first_sector() override;
	void set_dos_first_sector(disk::sector_num sector) override;
};

