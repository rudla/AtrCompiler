# AtrCompiler
Tool to create Atari disk images (ATR files).

## Usage

AtrCompiler can unpack contents of a ATR disk to a folder as separate files and create a dir file, which describes the contents of the disk, its size etc.
You can then modify the files and pack them to a new ATR disk.

```
AtrCompiler list   atr_file
AtrCompiler pack   atr_file [dir_file]
AtrCompiler unpack atr_file [dir_file]
```


## Dir file
Directory file describes format and contents of the created disk. It is composed on commands. Every command is on separate line.

```
DISK  <sector size> <number of sectors>
```
Specifies the size of the disk.

```
BOOT  filename
```
The disk will be bootable. The file specified must contain 3 sectors that will be used for booting.

```
FORMAT  filesystem [dos2]
```
Format the disk for use with specified filesystem.
Possible values are:
 * 2.0
 * 2.5
 * II+

```
[BIN|DOS|] filename [atarifilename]
```

File commands may start with optional format specifier (may be ommited, in which case it is BIN).
Filename may be followed by another filename, which specifies how should be the file named on the Atari disk.

```
--- atarifilename
```
Special --- format means, the file is empty (there is no file) and only empty directory entry should be created.

## Atari filename format

If necessary, we can specify filename, as it should appear on atari. We can \ as escape characters with following meaning:

```
\i   turn character inversion on/off
\xhh ATASCII character in hex
\<spc> space
\\   backslash
```
