#pragma once

#include "filesystem.h"

filesystem * detect_filesystem(disk * d);
filesystem * install_filesystem(disk * d, const std::string & dos_type);
