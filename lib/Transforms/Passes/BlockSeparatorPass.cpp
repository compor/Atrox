//
//
//

#include "Atrox/Config.hpp"

#include "Atrox/Util.hpp"

#include "Atrox/Debug.hpp"

#include "Atrox/Transforms/Passes/BlockSeparatorPass.hpp"

#include "Atrox/Transforms/BlockSeparator.hpp"

#include "private/PassCommandLineOptions.hpp"

#include "llvm/Pass.h"
// using llvm::RegisterPass

#include "llvm/IR/Instruction.h"
// using llvm::Instruction

#include "llvm/IR/Function.h"
// using llvm::Function

#include "llvm/IR/LegacyPassManager.h"
// using llvm::PassManagerBase

#include "llvm/Transforms/IPO/PassManagerBuilder.h"
// using llvm::PassManagerBuilder
// using llvm::RegisterStandardPasses

#include "llvm/ADT/SmallPtrSet.h"
// using llvm::SmallPtrSet

#include "llvm/Support/CommandLine.h"
// using llvm::cl::opt
// using llvm::cl::desc
// using llvm::cl::init
// using llvm::cl::ParseEnvironmentOptions
// using llvm::cl::ResetAllOptionOccurrences

#include "llvm/Support/Debug.h"
// using LLVM_DEBUG macro
// using llvm::dbgs

#include <algorithm>
// using std::find

#include <string>
// using std::string

#define DEBUG_TYPE ATROX_BLOCKSEPARATOR_PASS_NAME
#define PASS_CMDLINE_OPTIONS_ENVVAR "BLOCKSEPARATOR_CMDLINE_OPTIONS"

// plugin registration for opt

char atrox::BlockSeparatorLegacyPass::ID = 0;

static llvm::RegisterPass<atrox::BlockSeparatorLegacyPass>
    X(DEBUG_TYPE, PRJ_CMDLINE_DESC("block separator pass"), false, false);

// plugin registration for clang

// the solution was at the bottom of the header file
// 'llvm/Transforms/IPO/PassManagerBuilder.h'
// create a static free-floating callback that uses the legacy pass manager to
// add an instance of this pass and a static instance of the
// RegisterStandardPasses class

static void
registerBlockSeparatorLegacyPass(const llvm::PassManagerBuilder &Builder,
                                 llvm::legacy::PassManagerBase &PM) {
  PM.add(new atrox::BlockSeparatorLegacyPass());

  return;
}

static llvm::RegisterStandardPasses RegisterBlockSeparatorLegacyPass(
    llvm::PassManagerBuilder::EP_EarlyAsPossible,
    registerBlockSeparatorLegacyPass);

//

namespace atrox {

// new passmanager pass

BlockSeparatorPass::BlockSeparatorPass() {
  llvm::cl::ResetAllOptionOccurrences();
  llvm::cl::ParseEnvironmentOptions(DEBUG_TYPE, PASS_CMDLINE_OPTIONS_ENVVAR);
}

bool BlockSeparatorPass::perform(llvm::Function &F) {
  bool hasChanged = false;

  if (F.isDeclaration()) {
    return hasChanged;
  }

  if (AtroxFunctionWhiteList.size()) {
    auto found =
        std::find(AtroxFunctionWhiteList.begin(), AtroxFunctionWhiteList.end(),
                  std::string{F.getName()});

    if (found == AtroxFunctionWhiteList.end()) {
      return hasChanged;
    }
  }

  LLVM_DEBUG(llvm::dbgs() << "processing func: " << F.getName() << '\n';);

  return hasChanged;
}

llvm::PreservedAnalyses
BlockSeparatorPass::run(llvm::Function &F, llvm::FunctionAnalysisManager &FAM) {
  bool hasChanged = perform(F);

  return hasChanged ? llvm::PreservedAnalyses::all()
                    : llvm::PreservedAnalyses::none();
}

// legacy passmanager pass

void BlockSeparatorLegacyPass::getAnalysisUsage(llvm::AnalysisUsage &AU) const {
}

bool BlockSeparatorLegacyPass::runOnFunction(llvm::Function &F) {
  BlockSeparatorPass pass;

  return pass.perform(F);
}

} // namespace atrox

