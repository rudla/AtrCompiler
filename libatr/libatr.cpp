#include "libatr.h"

#include "dos2_filesystem.h"
#include "dos_IIplus.h"
#include "sparta_dos.h"
#include "mydos.h"

filesystem * detect_filesystem(disk * d)
{
	filesystem * fs;
	if (dos_IIplus::detect(d)) {
		fs = new dos_IIplus(d);
	} else if (sparta_dos::detect(d)) {
		fs = new sparta_dos(d);
	} else if (mydos::detect(d)) {
		fs = new mydos(d);
	} else {
		fs = new dos2(d);
	}
	return fs;
}

filesystem * install_filesystem(disk * d, const std::string & dos_type)
{
	if (dos_type == "II+") {
		return dos_IIplus::format(d);
	} else if (dos_type == "2.5" || dos_type == "2.0") {
		return dos2::format(d);
	} else if (dos_type == "sparta") {
		return sparta_dos::format(d);
	} else {
		throw "Unknown dos format";
	}
}