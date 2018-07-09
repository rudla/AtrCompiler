#include <iostream>
#include <fstream>
#include <vector>
#include <sstream>
#include "dos2_filesystem.h"

using namespace std;

/*

filename  source format filename

   \i   turn inversion on/off
   \xhh hex char
   \_   space
   \\   backslash

   */

disk * pack(const string dir_filename)
{

	ifstream index(dir_filename);

	auto d = new disk(128, 1040);

	filesystem * fs = dos2::format(d);

	while (!index.eof()) {
		string line;
		getline(index, line);
		if (line.size() == 0) continue;

		istringstream s(line);
		string atari_name;

		// parse atari name

		char name[12];
		bool inverse = false;
		for (size_t i = 0; i < sizeof(name); i++) name[i] = ' ';
		name[11] = 0;
		size_t i = 0;
		char c;
		bool atascii = false;

		while (s.get(c)) {
			if (c == ' ') break;
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
				atari_name.push_back(c);
				i = 8;
				continue;
			}
			if (i == 12) {
				throw "filename too long";
			}
			atari_name.push_back(c);
			name[i] = c | (inverse ? 128 : 0);
			i++;
		}

		string filename;

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
	}
	return d;
}

void unpack(filesystem * fs, const string & dir_file)
{
	ofstream atrdir;
	if (!dir_file.empty()) {
		atrdir.open(dir_file);
	}

	auto it = fs->root_dir();
	int name_idx = 1;
	for (; !it->at_end(); it->next()) {
		string name = it->name();

		cout << name << " " << it->sec_size() << "\n";

		if (atrdir.is_open()) {
			atrdir << name;
			if (it->sec_size() == 0) {
				atrdir << " ---";
			} else {
				if (name.find('\\') != string::npos) {
					ostringstream s;
					s << "F" << name_idx;
					auto p = name.find('.');
					if (p != string::npos) {
						s << name.substr(p);
					}
					name = s.str();
					atrdir << " bin " << name;
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
