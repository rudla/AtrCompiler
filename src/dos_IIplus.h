/*
DOS II+


Full sector size is used.

## Boot sector

$700   DOS version = 'X'
$709   XBUF (number of buffers)

XOPT = $70B Optionen 

Dieses Byte steuert einige spezielle Funktionen des XDOS, und zwar wie folgt (alle Werte sind hexadezimal angegeben): 
+20 RAM-Disk D8: beim Booten nicht löschen +10 Disks in mittlere Dichte beim Formatieren/Löschen MyDOS-kompatibel einrichten (statt DOS 2.5-kompatibel) +02 Titelinfos beim Aufruf des DUP nicht anzeigen +01 Im DUP das Directory immer automatisch zweispaltig anzeigen
Standardmäßig steht dieses Byte auf 00. Zum Ändern die gewünschten Optionen addieren und in $70B eintragen.

XCOL = $710 Text-Hintergrundfarbe 

Ist dieses Byte nicht 0, legt es die Text-Hintergrundfarbe fest. 0 bedeutet die Standardhintergrundfarbe.

## VTOC

VTOC is in sectors 359, 360

360    0     disk version (=3)
       1..2  disk capacity (1009 for medium density)
       3..4  number of free sectors (1009 at the start)
	   5..10 unused

359    0..9  additional 10 bytes of VTOC

*/


#pragma once

#include "dos2_filesystem.h"

class dos_IIplus : public dos2
{
public:
	dos_IIplus(disk * d);

	std::string name();

	static filesystem * format(disk * d);

	void vtoc_init();
	void vtoc_format();
	void vtoc_read();
	void vtoc_write();

	disk::sector_num alloc_sector();

	bool enhanced_vtoc();
};


/*

DISKCMD: Processing command: Unit 32, Command 53, Aux data 68 01 Get Status
DISKCMD: Processing command: Unit 32, Command 52, Aux data 69 01 Read Sector
DISKCMD: Processing command: Unit 32, Command 52, Aux data 68 01 Read Sector
DISKCMD: Processing command: Unit 32, Command 53, Aux data 68 01 Get Status
DISKCMD: Processing command: Unit 32, Command 4F, Aux data 68 01 Write PERCOM Block
DISKCMD: Processing command: Unit 32, Command 22, Aux data 68 01 Format Medium Density
DISKCMD: Processing command: Unit 32, Command 53, Aux data 68 01 Get Status
DISKCMD: Processing command: Unit 32, Command 50, Aux data 70 01 Write Sector 368
DISKCMD: Processing command: Unit 32, Command 50, Aux data 6F 01
DISKCMD: Processing command: Unit 32, Command 50, Aux data 6E 01
DISKCMD: Processing command: Unit 32, Command 50, Aux data 6D 01
DISKCMD: Processing command: Unit 32, Command 50, Aux data 6C 01
DISKCMD: Processing command: Unit 32, Command 50, Aux data 6B 01
DISKCMD: Processing command: Unit 32, Command 50, Aux data 6A 01
DISKCMD: Processing command: Unit 32, Command 50, Aux data 69 01
DISKCMD: Processing command: Unit 32, Command 50, Aux data 68 01
DISKCMD: Processing command: Unit 32, Command 50, Aux data 67 01 Write sector 359
DISKCMD: Processing command: Unit 32, Command 50, Aux data 01 00 Boot
DISKCMD: Processing command: Unit 32, Command 50, Aux data 02 00
DISKCMD: Processing command: Unit 32, Command 50, Aux data 03 00

*/