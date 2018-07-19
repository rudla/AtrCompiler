#include "filesystem.h"
#include <iostream>
#include <fstream>

using namespace std;

disk * filesystem::get_disk()
{
	return d;
}

void filesystem::file::write(byte * data, size_t size)
{
	while (size--) write(*data++);
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
		set_word(buf, prop->offset, value);
	}
	write_sector(prop->sector, buf);
	delete[] buf;
}

byte filesystem::get_byte(disk::sector_num sector, size_t offset)
{
	auto buf = new byte[sector_size()];
	read_sector(sector, buf);
	byte b = buf[offset];
	delete[] buf;
	return b;
}

size_t filesystem::get_word(disk::sector_num sector, size_t lo_offset, size_t hi_offset)
{
	byte lo = get_byte(sector, lo_offset);
	byte hi = get_byte(sector, hi_offset);
	return size_t(hi) * 256 + lo;
}

byte filesystem::get_property_byte(const property * prop)
{
	return get_byte(prop->sector, prop->offset);
}

string filesystem::get_property(const property * prop)
{
	char txt[32];
	size_t t;
	txt[0] = 0;

	if (prop->size == 1) {
		t = get_byte(prop->sector, prop->offset);
		txt[0] = '$';
		char h[3];
		_itoa_s<3>((int)t, h, 16);		
		txt[1] = h[0];
		txt[2] = h[1];
		txt[3] = 0;
	}
	return string(txt);
}