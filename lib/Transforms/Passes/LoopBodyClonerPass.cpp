//
//
//

#include "Atrox/Config.hpp"

#include "Atrox/Util.hpp"

#include "Atrox/Debug.hpp"

#include "Atrox/Analysis/NaiveSelector.hpp"

#include "Atrox/Transforms/Passes/LoopBodyClonerPass.hpp"

#include "Atrox/Transforms/LoopBodyCloner.hpp"

#include "private/PassCommandLineOptions.hpp"

#include "llvm/Pass.h"
// using llvm::RegisterPass

#include "llvm/IR/Instruction.h"
// using llvm::Instruction

#include "llvm/IR/Module.h"
// using llvm::Module

#include "llvm/IR/LegacyPassManager.h"
// using llvm::PassManagerBase

#include "llvm/Transforms/IPO/PassManagerBuilder.h"
// using llvm::PassManagerBuilder
// using llvm::RegisterStandardPasses

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

#include <iterator>
// using std::begin
// using std::end

#define DEBUG_TYPE ATROX_LOOPBODYCLONER_PASS_NAME
#define PASS_CMDLINE_OPTIONS_ENVVAR "LOOPBODYCLONER_CMDLINE_OPTIONS"

// plugin registration for opt

char atrox::LoopBodyClonerLegacyPass::ID = 0;

static llvm::RegisterPass<atrox::LoopBodyClonerLegacyPass>
    X(DEBUG_TYPE, PRJ_CMDLINE_DESC("loop body clone pass"), false, false);

// plugin registration for clang

// the solution was at the bottom of the header file
// 'llvm/Transforms/IPO/PassManagerBuilder.h'
// create a static free-floating callback that uses the legacy pass manager to
// add an instance of this pass and a static instance of the
// RegisterStandardPasses class

static void
registerLoopBodyClonerLegacyPass(const llvm::PassManagerBuilder &Builder,
                                 llvm::legacy::PassManagerBase &PM) {
  PM.add(new atrox::LoopBodyClonerLegacyPass());

  return;
}

static llvm::RegisterStandardPasses RegisterLoopBodyClonerLegacyPass(
    llvm::PassManagerBuilder::EP_EarlyAsPossible,
    registerLoopBodyClonerLegacyPass);

//

enum class SelectionStrategy { Naive, IteratorRecognitionBased };

static llvm::cl::bits<SelectionStrategy> SelectionStrategyOption(
    "atrox-selection-strategy", llvm::cl::desc("block selection strategy"),
    llvm::cl::values(clEnumValN(SelectionStrategy::Naive, "naive", "naive"),
                     clEnumValN(SelectionStrategy::IteratorRecognitionBased,
                                "itr", "iterator recognition based")),
    llvm::cl::CommaSeparated, llvm::cl::cat(AtroxCLCategory));

static void checkAndSetCmdLineOptions() {
  if (!SelectionStrategyOption.getBits()) {
    SelectionStrategyOption.addValue(SelectionStrategy::Naive);
  }
}

//

namespace atrox {

// new passmanager pass

LoopBodyClonerPass::LoopBodyClonerPass() {
  llvm::cl::ResetAllOptionOccurrences();
  llvm::cl::ParseEnvironmentOptions(DEBUG_TYPE, PASS_CMDLINE_OPTIONS_ENVVAR);

  checkAndSetCmdLineOptions();
}

bool LoopBodyClonerPass::run(llvm::Module &M) {
  bool hasChanged = false;

  for (auto &func : M) {
    if (func.isDeclaration()) {
      continue;
    }

    if (FunctionWhiteList.size()) {
      auto found = std::find(FunctionWhiteList.begin(), FunctionWhiteList.end(),
                             std::string{func.getName()});

      if (found == FunctionWhiteList.end()) {
        continue;
      }
    }

    LoopBodyCloner lpc{M};
    llvm::LoopInfo li{llvm::DominatorTree(const_cast<llvm::Function &>(func))};
    NaiveSelector ns;
    hasChanged |= lpc.cloneLoops(li, ns);
  }

  return hasChanged;
}

llvm::PreservedAnalyses
LoopBodyClonerPass::run(llvm::Module &M, llvm::ModuleAnalysisManager &MAM) {
  bool hasChanged = run(M);

  return hasChanged ? llvm::PreservedAnalyses::all()
                    : llvm::PreservedAnalyses::none();
}

// legacy passmanager pass

void LoopBodyClonerLegacyPass::getAnalysisUsage(llvm::AnalysisUsage &AU) const {
  AU.setPreservesAll();
}

bool LoopBodyClonerLegacyPass::runOnModule(llvm::Module &M) {
  LoopBodyClonerPass pass;

  return pass.run(M);
}

} // namespace atrox
