#include "rkdos.h"

using namespace std;

disk::sector_num rkdos::root_sec = 1;

std::string rkdos::name()
{
	return "rkdos";
}


const filesystem::property * rkdos::properties()
{
	static const filesystem::property props[1] = {
		{ nullptr, 0, 0, 0 }
	};

	return props;
}

rkdos::rkdos(disk * d) : filesystem(d)
{
	auto s = d->get_sector(root_sec);
	free_start = s->dpeek(root_first_free);
	free_size = s->dpeek(root_free_size);
}

rkdos::~rkdos()
{
	auto s = d->get_sector(root_sec);
	s->dpoke(root_first_free, word(free_start));
	s->dpoke(root_free_size, free_size);
}

filesystem * rkdos::format(disk * d)
{
	auto s = d->init_sector(root_sec);
	s->poke(root_id1, 'R');
	s->poke(root_id2, 'K');

	//fs->free_start = 4;
	//fs->free_size = word(d->sector_count() - 3);

	s->dpoke(root_first_free, 4);	//word(fs->free_start)
	s->dpoke(root_free_size, word(d->sector_count() - 3)); // fs->free_size

	s->poke(root_entry + dir_entry_size, dir_name);
	s->poke(root_entry + dir_flags, file_dir);
	s->dpoke(root_entry + dir_cluster_start, 0);
	s->poke(root_entry + dir_cluster_size, 0);
	s->dpoke(root_entry + dir_file_size, 0);
	s->poke(root_entry + dir_file_size+2, 0);

	auto fs = new rkdos(d);

	return fs;
}

bool rkdos::detect(disk * d)
{
	auto s = d->get_sector(root_sec);
	return s->peek(root_id1) == 'R' && s->peek(root_id2) == 'K';
}


filesystem::dir * rkdos::root_dir()
{
	auto s = get_sector(root_sec);

	auto root_file = new rkdos_file(*this, nullptr, root_entry, root_sec, 1, 32, false);


	rkdos_file * file = new rkdos_file(*this, root_file, root_entry, s->dpeek(root_entry+dir_cluster_start), s->peek(root_entry + dir_cluster_size), s->dpeek(root_entry + dir_file_size), false);
	rkdos_dir * dir = new rkdos_dir(file);
	return dir;
}

disk::sector_num rkdos::alloc_sector()
{
	auto r = free_start;

	if (free_size > 0) {
		free_size--;
		free_start++;
		return r;
	}
	return 0;
}

disk::sector_num rkdos::free_sector_count()
{
	return 0;
}

disk::sector_num rkdos::get_dos_first_sector()
{
	return 0;
}

void rkdos::set_dos_first_sector(disk::sector_num sector)
{
}

//////////////////////// file /////////////////////////////

rkdos::rkdos_file::rkdos_file(rkdos & fs, rkdos_file * dir, word dir_pos,  disk::sector_num cluster_start, byte cluster_size, size_t file_size, bool writing) : fs(fs), file_size(file_size), first_cluster(cluster_start), dir(dir), dir_pos(dir_pos)
{
	cluster_start = first_cluster;
	first_cluster_size = cluster_size;
	cluster_end = first_cluster + cluster_size - 1;
	sector = first_cluster;
	offset = 0;
	file_pos = 0;
}

rkdos::rkdos_file::~rkdos_file()
{

	if (first_cluster_size == 0) {
		first_cluster_size = word(cluster_end - first_cluster + 1);
	}

	byte buf[7];
	dir->seek(dir_pos + dir_cluster_start);
	poke_word(buf, 0, word(first_cluster));
	buf[2] = byte(first_cluster_size);

	//poke_word(buf, 2, word(first_cluster_size));
	poke_word(buf, 3, word(file_size));
	buf[5] = byte(file_size >> 16);
	dir->write_bytes(buf, 6);
}

rkdos & rkdos::rkdos_file::filesystem()
{
	return fs;
}

bool rkdos::rkdos_file::eof()
{
	return file_size == file_pos;
}

byte rkdos::rkdos_file::read()
{
	if (eof()) throw("EOF");

	auto s = fs.get_sector(sector);
	byte b = s->peek(offset);
	file_pos++;
	offset++;
	if (offset == fs.sector_size()) {
		offset = 0;
		sector++;
	}
	return b;
}

void rkdos::rkdos_file::seek(size_t pos)
{
	
	sector = first_cluster;
	offset = 0;
	word cluster_size = first_cluster_size;
	auto remaining = file_size;

	if (pos > file_size) pos = file_size;

	size_t cluster_bytes = cluster_size * fs.sector_size();

	if (cluster_bytes > remaining) {
		cluster_bytes -= 2;
	}

	sector += pos / fs.sector_size();
	offset = word(pos % fs.sector_size());
	file_pos = pos;

	cluster_end = sector;
	cluster_end_offset = word(fs.sector_size());
}

void rkdos::rkdos_file::seek_end()
{
	sector = first_cluster+first_cluster_size;
	cluster_end = sector;
	cluster_end_offset = 0;
	offset = 0;
	file_pos = file_size;
}

void rkdos::rkdos_file::write(byte b)
{
	if (first_cluster == 0) {
		first_cluster = fs.alloc_sector();
		sector = first_cluster;
		cluster_end = sector;
		cluster_end_offset = word(fs.sector_size());
	}

	auto s = fs.get_sector(sector);

	// if we are at the end of cluster, we must alloc new block -- note, thet we are not currently solving overwrite

	if (offset == cluster_end_offset && sector == cluster_end) {
		
		sector = fs.alloc_sector();
	
		if (sector == 0) throw "disk full";
		
		cluster_end++;
		offset = 0;

		if (sector == cluster_end) {
		} else {
			if (first_cluster_size == 0) first_cluster_size = word(cluster_end - first_cluster);
			byte b1, b2;
			b1 = s->peek(fs.sector_size() - 2);
			b2 = s->peek(fs.sector_size() - 1);
			s->dpoke(fs.sector_size(), word(sector));
			write(b1);
			write(b2);
			return;
		}
	}

	s->poke(offset, b);
	offset++;
	file_pos++;
	if (file_pos > file_size) {
		file_size = file_pos;
	}

}

disk::sector_num rkdos::rkdos_file::first_sector()
{
	return first_cluster;
}

rkdos::rkdos_dir::rkdos_dir(rkdos_file * file) : file(file) {
	pos = 0;
	entry_size = 0;
	if (!file->eof()) {
		next();
		end = false;
	} else {
		end = true;
	}
	//file->set_size(size());
	//next();
}

rkdos::rkdos_dir::~rkdos_dir() {
	delete file;
}

bool rkdos::rkdos_dir::at_end()
{
	return end;
	//return file->eof();
}

void rkdos::rkdos_dir::next()
{
	if (file->eof()) {
		end = true;
	} else {
		pos += entry_size;
		entry_size = file->read();
		buf[0] = entry_size;
		((filesystem::file *)file)->read(buf + 1, entry_size);
		entry_size++;
	}
}

filesystem::dir * rkdos::rkdos_dir::open_dir()
{
//	return file->filesystem().open_dir(first_sector());
	return nullptr;
}

bool  rkdos::rkdos_dir::is_dir()
{
	return (buf[dir_flags] & file_dir) != 0;
}

bool  rkdos::rkdos_dir::is_deleted()
{
	//return (buf[0] & FLAG_DELETED) != 0;
	return false;
}

std::string rkdos::rkdos_dir::name()
{
	char t[4 * 24];
	bool inverse = false;
	byte * filename = buf + dir_name;
	byte len = buf[dir_entry_size] - dir_name;

	size_t p = 0, non_space = 0;
	for (size_t i = 0; i < 11; i++) {
		byte b = filename[i];
		if (b >= 128) {
			if (!inverse) {
				inverse = true;
				t[p++] = '\\'; t[p++] = 'i';
			}
			b = b & 0x7f;
		} else {
			if (inverse) {
				t[p++] = '\\'; t[p++] = 'i';
				inverse = false;
			}
		}
		if (b < 32 || b == 123 || b >= 125) {
			const char * hex_digit = "0123456789ABCDEF";
			t[p++] = '\\'; t[p++] = 'x'; t[p++] = hex_digit[b >> 4]; t[p++] = hex_digit[b & 0xf];
		} else {
			if (b == ' ' || b == '\\') t[p++] = '\\';
			char c = char(b);
			t[p++] = c;
		}
		if (b != ' ' || inverse) non_space = p;
	}
	t[non_space] = 0;
	return string(t);
}

size_t rkdos::rkdos_dir::size()
{
	return  peek_word(buf, dir_file_size) + 0x10000 * buf[dir_file_size + 2];
}

disk::sector_num rkdos::rkdos_dir::cluster_start()
{
	return peek_word(buf, dir_cluster_start);
}

byte rkdos::rkdos_dir::cluster_size()
{
	return buf[dir_cluster_size];
}

size_t rkdos::rkdos_dir::sec_size()
{
	auto sec_size = file->filesystem().sector_size();
	auto sz = size() + (sec_size - 1);
	return sz / sec_size;
}

filesystem::file * rkdos::rkdos_dir::open_file()
{
	return new rkdos_file(file->filesystem(), file, 0, cluster_start(), cluster_size(), size(), false);
}

filesystem::file * rkdos::rkdos_dir::create_file(char * name)
{
	byte buf[48];
	memset(buf, 0, sizeof(buf));

	byte name_len = byte(strlen(name));
	buf[dir_entry_size] = name_len + dir_name - 1;
	buf[dir_flags] = 0;
	memcpy(&buf[dir_name], name, name_len);

	file->seek_end();
	file->write_bytes(buf, size_t(buf[0]+1));

	auto f = new rkdos_file(file->filesystem(), file, 0, 0, 0, 0, true);

	return f;
}
