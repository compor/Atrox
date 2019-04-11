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

llvm::cl::opt<std::string>
    AtroxReportsDir("atrox-reports-dir",
                    llvm::cl::desc("output reports directory"),
                    llvm::cl::cat(AtroxCLCategory));

llvm::cl::list<std::string> AtroxFunctionWhiteList(
    "atrox-func-wl", llvm::cl::Hidden,
    llvm::cl::desc("process only the specified functions"),
    llvm::cl::cat(AtroxCLCategory));

