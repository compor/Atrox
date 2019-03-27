//
//
//

#include "Atrox/Config.hpp"

#include "Atrox/Util.hpp"

#include "Atrox/Transforms/Passes/LoopBodyClonerPass.hpp"

#include "llvm/IR/PassManager.h"
// using llvm::ModuleAnalysisManager

#include "llvm/Passes/PassBuilder.h"
// using llvm::PassBuilder

#include "llvm/Passes/PassPlugin.h"
// using llvmGetPassPluginInfo

#include "llvm/ADT/StringRef.h"
// using llvm::StringRef

#include "llvm/Support/Debug.h"
// using LLVM_DEBUG macro
// using llvm::dbgs

#define DEBUG_TYPE "atrox-plugin-registration"

// plugin registration for opt new passmanager

namespace {

bool parseModulePipeline(llvm::StringRef Name, llvm::ModulePassManager &FPM,
                         llvm::ArrayRef<llvm::PassBuilder::PipelineElement>) {
#define MODULE_PASS(NAME, CREATE_PASS)                                         \
  if (Name == NAME) {                                                          \
    LLVM_DEBUG(llvm::dbgs()                                                    \
                   << "registering pass parser for " << NAME << "\n";);        \
    FPM.addPass(CREATE_PASS);                                                  \
    return true;                                                               \
  }

#include "Passes.def"

  return false;
}

void registerPasses(llvm::PassBuilder &PB) {
  PB.registerPipelineParsingCallback(parseModulePipeline);
}

} // namespace

extern "C" ::llvm::PassPluginLibraryInfo LLVM_ATTRIBUTE_WEAK
llvmGetPassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "AtroxPlugin", STRINGIFY(VERSION_STRING),
          registerPasses};
}

