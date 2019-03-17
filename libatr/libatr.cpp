#include "libatr.h"

#include "dos2_filesystem.h"
#include "dos_2_5.h"
#include "dos_IIplus.h"
#include "xdos.h"
#include "sparta_dos.h"
#include "mydos.h"
#include "rkdos.h"

filesystem * detect_filesystem(disk * d)
{
	filesystem * fs;
	if (xdos::detect(d)) {
		fs = new xdos(d);
	} else if(dos_IIplus::detect(d)) {
		fs = new dos_IIplus(d);
	} else if (rkdos::detect(d)) {
		fs = new rkdos(d);
	} else if (sparta_dos::detect(d)) {
		fs = new sparta_dos(d);
	} else if (mydos::detect(d)) {
		fs = new mydos(d);
	} else if (dos2::detect(d)) {
		fs = new dos2(d);
	} else {
		fs = new dos25(d);
	}
	return fs;
}

filesystem * install_filesystem(disk * d, const std::string & dos_type)
{
	if (dos_type == "xdos") {
		return xdos::format(d);
	} else if (dos_type == "II+") {
		return dos_IIplus::format(d);
	} else if (dos_type == "2") {
		return dos2::format(d);
	} else if (dos_type == "2.5" || dos_type == "2.0") {
		return dos25::format(d);
	} else if (dos_type == "sparta") {
		return sparta_dos::format(d);
	} else if (dos_type == "mydos") {
		return mydos::format(d);
	} else if (dos_type == "rkdos") {
		return rkdos::format(d);
	} else {
		throw "Unknown dos format";
	}
}