# AtrCompiler
Tool to create Atari disk images (ATR files).


## Dir file
Directory file describes format and contents of the created disk. It is composed on commands. Every command is on separate line.

```
DISK  <sector size> <number of sectors>
BOOT  filename
FORMAT  filesystem [dos2]

[BIN|DOS|] filename [atarifilename]

--- atarifilename
```

File commands may start with optional format specifier (may be ommited, in which case it is BIN).
Filename may be followed by another filename, which specifies how should be the file named on the Atari disk.

Special --- format means, the file is empty (there is no file) and only empty directory entry should be created.

## Atari filename format

If necessary, we can specify filename, as it should appear on atari. We can \ as escape characters with following meaning:

```
\i   turn character inversion on/off
\xhh ATASCII character in hex
\<spc> space
\\   backslash
```
