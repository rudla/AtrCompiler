#include <iostream>
#include <fstream>
#include <vector>
#include <sstream>
#include <memory>
#include "dos2_filesystem.h"
#include "dos_IIplus.h"

using namespace std;

/*

Index file
==========

DISK  density
BOOT  filename
FS    filesystem [dos2]

filename [atarifilename]
--- atarifilename
BIN filename atarifilename
DOS filename atarifilename -- will install the file as DOS.SYS

   */

enum class file_format {
	empty,
	bin,
	dos
};

bool format_atari_name(istringstream & s, char * name, size_t name_len, size_t ext_len)
/*
  Atari filename format

  \i   turn inversion on / off
  \xhh hex char
  \    space
  \\   backslash
*/{
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

filesystem * detect_filesystem(disk * d)
{
	filesystem * fs;
	if (dos_IIplus::detect(d)) {
		fs = new dos_IIplus(d);
	} else {
		fs = new dos2(d);
	}
	return fs;
}

disk * pack(const string dir_filename)
{

	ifstream index(dir_filename);

	size_t sector_size = 128;
	disk::sector_num sector_count = 1040;
	string dos_type;

	disk * d = nullptr;
	filesystem * fs = nullptr;
	const filesystem::property * prop;

	while (!index.eof()) {
		string line;
		getline(index, line);
		if (line.size() == 0) continue;

		istringstream s(line);

		//parse file name or command

		string filename;

		s >> filename;

		file_format fformat = file_format::bin;

		if (filename == "DISK") {
			s >> sector_count >> sector_size;
			continue;

		} else if (filename == "FORMAT") {
			s >> dos_type;
			if (!d) d = new disk(sector_size, sector_count);
			if (dos_type == "II+") {
				fs = dos_IIplus::format(d);
			} else if (dos_type == "2.5" || dos_type == "2.0") {
				fs = dos2::format(d);
			} else {
				throw "Unknown dos format";
			}
			continue;

		} else if (filename == "BOOT") {
			if (!d) d = new disk(sector_size, sector_count);
			s >> filename;
			d->install_boot(filename);
			continue;

		} else if (fs && (prop = fs->find_property(filename))) {
			string value;
			getline(s, value);
			fs->set_property(prop, value);
			continue;
		}  else if (filename == "---") {
			fformat = file_format::empty;
		} else {
			if (filename == "BIN") {
				fformat = file_format::bin;
				s >> filename;
			} else if (filename == "DOS") {
				fformat = file_format::dos;
				s >> filename;
			} else {
				fformat = file_format::bin;
			}
		}

		// parse atari name

		char name[12];
		if (!format_atari_name(s, name, 8, 3)) {
			istringstream fs(filename);
			format_atari_name(fs, name, 8, 3);
		}

		auto file = fs->create_file(name);
		if (fformat != file_format::empty) {
			if (filename.size() == 0) {
				throw "no filename";
			}
			cout << filename << "\n";
			file->import(filename);
			if (fformat == file_format::dos) {
				auto pos = file->first_sector();
				fs->set_dos_first_sector(file->first_sector());
			}
		}
		delete file;
	}
	delete fs;
	return d;
}

void unpack(filesystem * fs, const string & dir_file)
{
	ofstream atrdir;
	if (!dir_file.empty()) {
		atrdir.open(dir_file);
	}

	cout << "DISK " << fs->sector_count() << " " << fs->sector_size() << "\n";
	cout << "FORMAT " << fs->name() << "\n";

	if (atrdir.is_open()) {
		atrdir << "DISK " << fs->sector_count() << " " << fs->sector_size() << "\n";
		atrdir << "FORMAT " << fs->name() << "\n";
	}

	string boot_filename = "boot.bin";
	atrdir << "BOOT " << boot_filename << "\n";
	fs->get_disk()->save_boot(boot_filename);

	for (auto prop = fs->properties(); prop->name; prop++) {
		string value = fs->get_property(prop);
		atrdir << prop->name << " " << value << "\n";
	}

	auto dos_first_sector = fs->get_dos_first_sector();

	auto it = fs->root_dir();
	int name_idx = 1;
	for (; !it->at_end(); it->next()) {
		string name = it->name();

		cout << name << " " << it->sec_size() << "\n";

		if (atrdir.is_open()) {
			if (it->sec_size() == 0) {
				atrdir << "--- ";
				atrdir << name;
			} else {

				auto file = it->open_file();
				
				if (file->first_sector() == dos_first_sector) {
					atrdir << "DOS ";
				}

				if (name.find('\\') != string::npos) {
					ostringstream s;
					s << "F" << name_idx << ".bin";
					auto p = name.find('.');
					if (p != string::npos) {
						s << name.substr(p);
					}
					atrdir << s.str() << " " << name;
					name = s.str();					
				} else {
					atrdir << name;
				}

				file->save(name);
				delete file;
			}
			atrdir << "\n";
		}
	}
	delete it;
}

const string help =
"AtrCompiler v0.5\n"
"\n"
"Usage:\n"
"AtrCompiler list   atr_file\n"
"AtrCompiler pack   atr_file [dir_file]\n"
"AtrCompiler unpack atr_file [dir_file]\n"
"\n";

int main(int argc, char *argv[])
{
	/*

	pack   .atr [dirfile]
	unpack .atr [dirfile]
	list   .atr

	*/
	string command;
	int x = 1;
	if (argc == 1) {
		cout << help;
	} else {
		if (strcmp(argv[x], "list") == 0) {
			x++;
			auto d = disk::load(argv[x++]);
			auto fs = detect_filesystem(d);
			unpack(fs, "");
		} else if (strcmp(argv[x], "pack") == 0) {
			x++;
			string atr = argv[x++];
			string dir = "dir.txt";
			if (x < argc) {
				dir = argv[x++];
			}
			auto d2 = pack(dir);
			d2->save(atr);
			delete d2;
		} else if (strcmp(argv[x], "unpack") == 0) {
			x++;
			string atr = argv[x++];
			string dir = "dir.txt";
			if (x < argc) {
				dir = argv[x++];
			}
			auto d = disk::load(atr);
			auto fs = detect_filesystem(d);
			unpack(fs, dir);
		}
	}
	return 0;
}
