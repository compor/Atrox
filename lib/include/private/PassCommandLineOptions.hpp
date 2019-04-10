//
//
//

#pragma once

#include "Atrox/Config.hpp"

#include "llvm/Support/CommandLine.h"
// using llvm::cl::OptionCategory

#include <cstdint>
// using uint64_t

#include <string>
// using std::string

extern llvm::cl::OptionCategory AtroxCLCategory;

extern llvm::cl::opt<std::string> ReportsDir;

extern llvm::cl::list<std::string> FunctionWhiteList;

