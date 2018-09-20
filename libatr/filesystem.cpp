#include "filesystem.h"
#include <iostream>
#include <fstream>

using namespace std;

disk * filesystem::get_disk()
{
	return d;
}

void filesystem::file::write(const byte * data, size_t size)
{
	while (size--) write(*data++);
}

void filesystem::file::read(byte * data, size_t size)
{
	while (size--) *data++ = read();
}

void filesystem::file::save(const string & filename)
{
	ofstream o(filename, ios::binary);
	while (!eof()) {
		char c = read();
		o << c;
	}
}

void filesystem::file::import(const string & filename)
{
	ifstream o(filename, ios::binary);
	if (!o.is_open()) throw "file does not exist";
	byte c;
	while (o.read((char *)&c, 1)) {
		write(c);
	}
}

const filesystem::property * filesystem::find_property(const std::string & name)
{
	
	for (auto p = properties(); p->name; p++) {
		if (name == p->name) return p;
	}
	return nullptr;
}

void filesystem::set_property(const property * prop, const string & value)
{
	auto t = value.c_str();

	while (*t == ' ') t++;
	int c;
	if (*t == '$') {
		t++;
		c = std::stoi(t, nullptr, 16);
	} else {
		c = std::stoi(t, nullptr, 10);
	}
	set_property(prop, c);
}

void filesystem::set_property(const property * prop, int value)
{
	auto buf = new byte[sector_size()];
	read_sector(prop->sector, buf);
	if (prop->size == 1) {
		buf[prop->offset] = value;
	} else if (prop->size == 2) {
		poke_word(buf, prop->offset, value);
	}
	write_sector(prop->sector, buf);
	delete[] buf;
}

byte filesystem::read_byte(disk::sector_num sector, size_t offset)
{
	auto buf = new byte[sector_size()];
	read_sector(sector, buf);
	byte b = buf[offset];
	delete[] buf;
	return b;
}

size_t filesystem::read_word(disk::sector_num sector, size_t lo_offset, size_t hi_offset)
{
	byte lo = read_byte(sector, lo_offset);
	byte hi = read_byte(sector, hi_offset);
	return size_t(hi) * 256 + lo;
}

void filesystem::write_byte(disk::sector_num sector, size_t offset, byte val)
{
	auto buf = new byte[sector_size()];
	read_sector(sector, buf);
	buf[offset] = val;
	write_sector(sector, buf);
	delete[] buf;
}

void filesystem::write_word(disk::sector_num sector, size_t offset, size_t val)
{
	auto buf = new byte[sector_size()];
	read_sector(sector, buf);
	poke_word(buf, offset, val);
	write_sector(sector, buf);
	delete[] buf;
}

byte filesystem::get_property_byte(const property * prop)
{
	return read_byte(prop->sector, prop->offset);
}

string filesystem::get_property(const property * prop)
{
	char txt[32];
	size_t t;
	txt[0] = 0;

	if (prop->size == 1) {
		t = read_byte(prop->sector, prop->offset);
		txt[0] = '$';
		char h[3];
		sprintf(h, "%02x", (int)t);
		txt[1] = h[0];
		txt[2] = h[1];
		txt[3] = 0;
	} else if (prop->size > 2) {
		size_t i;
		for (i = 0; i < prop->size; i++) {
			txt[i] = read_byte(prop->sector, prop->offset + i);
		}
		txt[i] = 0;
	}
	return string(txt);
}

bool filesystem::format_atari_name(istream & s, char * name, size_t name_len, size_t ext_len)
/*
Atari filename format

\i   turn inversion on / off
\xhh hex char
\    space
\\   backslash
*/ {
	bool inverse = false;
	auto len = name_len + ext_len;

	for (size_t i = 0; i < len; i++) name[i] = ' ';
	name[len] = 0;
	size_t i = 0;
	char c;
	bool atascii = false;

	while (s.get(c)) {
		if (c == ' ') {
			if (i == 0) continue;
			else break;
		}
		if (c == '\\') {
			s.get(c);
			switch (c) {
			case 'i':
				inverse = !inverse;
				continue;
			case 'x':
				char h[3];
				s.get(h, 3);
				c = std::stoi(h, nullptr, 16);
				atascii = true;
				break;
			}
		} else if (c == '.' && i <= name_len) {
			i = name_len;
			continue;
		}
		if (i > len) {
			throw "filename too long";
		}
		name[i] = c | (inverse ? 128 : 0);
		i++;
	}
	return i > 0;
}

filesystem::dir * filesystem::dir::open_dir()
{
	throw "not_a_dir";
}

bool  filesystem::dir::is_dir()
{
	return false;
}

bool  filesystem::dir::is_deleted()
{
	return false;
}

disk::sector_num filesystem::free_sector_count()
{
	return 0;
}
