//
//
//

#pragma once

#include "Atrox/Config.hpp"

#include "llvm/Support/CommandLine.h"
// using llvm::cl::OptionCategory

#include <cstdint>
// using uint64_t

extern llvm::cl::OptionCategory AtroxCLCategory;

extern llvm::cl::list<std::string> FunctionWhiteList;

