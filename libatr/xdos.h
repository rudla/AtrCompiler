/*
XDOS


XDOS is enhanced version of DOS II+.

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

XDOS version is $58 ('X').


First sector for DOS is specified by following code:

0731: A0 04     LDY #$04
0733: A2 00     LDX #$00
0735: 8A        TXA
0736: 18        CLC
0737: 20 6C 07  JSR $76c

To specify the boot start, we must therefore set LO,HI to $0732, $0734. (Byte $32 and $34 on sector 1.)


*/


#pragma once

#include "dos_IIplus.h"

class xdos : public dos_IIplus
{
public:
	xdos(disk * d);

	std::string name();
	const property * properties();

	static filesystem * format(disk * d);
	static bool detect(disk * d);

	disk::sector_num get_dos_first_sector() override;
	void set_dos_first_sector(disk::sector_num sector) override;
};

