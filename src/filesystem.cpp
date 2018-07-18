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

void filesystem::set_dos(disk::sector_num sector)
{
	if (auto p = find_property(dos_lo)) {
		set_property(p, sector & 0xff);
	}
	if (auto p = find_property(dos_hi)) {
		set_property(p, (sector >> 8) & 0xff);
	}
}

byte filesystem::get_property_byte(const char * name)
{
	if (auto prop = find_property(name)) {
		auto buf = new byte[sector_size()];
		read_sector(prop->sector, buf);
		byte b = buf[prop->offset];
		delete[] buf;
		return b;
	}
	throw "unknown property";
}

disk::sector_num filesystem::get_dos()
{
	byte lo = 0;
	byte hi = 0;

	lo = get_property_byte(dos_lo);
	hi = get_property_byte(dos_hi);
	return ((int)hi * 256) + lo;
}
