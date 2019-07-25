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

llvm::cl::opt<bool>
    AtroxIgnoreAliasing("atrox-ignore-aliasing", llvm::cl::init(true),
                        llvm::cl::desc("use specific aliasing assumptions"),
                        llvm::cl::cat(AtroxCLCategory));

llvm::cl::opt<bool> AtroxSkipCalls("atrox-skip-regions-with-calls",
                                   llvm::cl::init(true),
                                   llvm::cl::desc("skip regions with calls"),
                                   llvm::cl::cat(AtroxCLCategory));

llvm::cl::opt<std::string>
    AtroxReportsDir("atrox-reports-dir",
                    llvm::cl::desc("output reports directory"),
                    llvm::cl::cat(AtroxCLCategory));

llvm::cl::opt<std::string> AtroxFunctionWhiteListFile(
    "atrox-func-wl-file", llvm::cl::Hidden,
    llvm::cl::desc("process only the specified functions"),
    llvm::cl::cat(AtroxCLCategory));

