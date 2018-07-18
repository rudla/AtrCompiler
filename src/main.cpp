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


Atari filename format

filename  source format filename

   \i   turn inversion on/off
   \xhh hex char
   \_   space
   \\   backslash

   */

enum class file_format {
	empty,
	bin,
	dos
};

bool parse_atari_name(istringstream & s, char * name, size_t len)
{
	bool inverse = false;
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
		} else if (c == '.' && i < 9) {
			i = 8;
			continue;
		}
		if (i == 12) {
			throw "filename too long";
		}
		name[i] = c | (inverse ? 128 : 0);
		i++;
	}
	return i > 0;
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
		if (!parse_atari_name(s, name, 11)) {
			istringstream fs(filename);
			parse_atari_name(fs, name, 11);
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
				fs->set_dos(file->first_sector());
			}
		}
		delete file;

/*
		s >> filename;

		int format = 2;
		if (filename == "---") {
			format = 0;
		} else if (filename == "tch") {
			filename.clear();
			s >> filename;
			format = 1;
		} else if (filename == "bin") {
			filename.clear();
			s >> filename;
			format = 2;
		} else if (filename == "boot") {
			filename.clear();
			s >> filename;
			format = 3;
		} else {
			format = 2;
		}

		if (filename.size() == 0 && !atascii) {
			filename = atari_name;
		}

		if (format == 3) {
			d->install_boot(filename);
		} else {
			auto file = fs->create_file(name);
			if (format != 0) {
				if (filename.size() == 0) {
					throw "no filename";
				}
				cout << filename << "\n";
				file->import(filename);
			}
			delete file;
		}
		*/
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

				auto file = it->open_file();
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
			auto fs = new dos2(d);
			unpack(fs, "");
		} else if (strcmp(argv[x], "pack") == 0) {
			x++;
			string atr = argv[x++];
			string dir = "atr.dir";
			if (x < argc) {
				dir = argv[x++];
			}
			auto d2 = pack(dir);
			d2->save(atr);
			delete d2;
		} else if (strcmp(argv[x], "unpack") == 0) {
			x++;
			string atr = argv[x++];
			string dir = "atr.dir";
			if (x < argc) {
				dir = argv[x++];
			}
			auto d = disk::load(atr);
			auto fs = new dos2(d);
			unpack(fs, dir);
		}
	}
	return 0;
}
