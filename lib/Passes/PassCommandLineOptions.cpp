//
//
//

#include "private/PassCommandLineOptions.hpp"

#include "llvm/Support/CommandLine.h"
// using llvm::cl::opt
// using llvm::cl::list
// using llvm::cl::desc
// using llvm::cl::location
// using llvm::cl::cat
// using llvm::cl::OptionCategory

#include <string>
// using std::string

llvm::cl::OptionCategory AtroxCLCategory("Atrox Pass",
                                             "Options for Atrox pass");

llvm::cl::list<std::string>
    FunctionWhiteList("atrox-func-wl", llvm::cl::Hidden,
                      llvm::cl::desc("process only the specified functions"),
                      llvm::cl::cat(AtroxCLCategory));

