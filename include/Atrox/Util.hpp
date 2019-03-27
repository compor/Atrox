//
//
//

#pragma once

#include "Atrox/Config.hpp"

// preprocessor stringification macros
#define STRINGIFY_UTIL(x) #x
#define STRINGIFY(x) STRINGIFY_UTIL(x)

#define PRJ_CMDLINE_DESC(x) x " (version: " STRINGIFY(VERSION_STRING) ")"

