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
