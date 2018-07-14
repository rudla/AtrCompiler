# AtrCompiler
Tool to create Atari disk images (ATR files).


## Dir file
Directory file describes format and contents of the created disk. It is composed on commands. Every command is on separate line.

```
DISK  <sector size> <number of sectors>
BOOT  filename
FORMAT  filesystem [dos2]

filename [atarifilename]

BIN filename atarifilename
DOS filename atarifilename
```

## Atari filename format

If necessary, we can specify filename, as it should appear on atari. We can \ as escape characters with following meaning:

```
\i   turn character inversion on/off
\xhh ATASCII character in hex
\<spc> space
\\   backslash
```
