//
//
//

#include "Atrox/Config.hpp"

#include "Atrox/Util.hpp"

#include "Atrox/Debug.hpp"

#include "Atrox/Analysis/NaiveSelector.hpp"

#include "Atrox/Analysis/IteratorRecognitionSelector.hpp"

#include "Atrox/Transforms/Passes/LoopBodyClonerPass.hpp"

#include "Atrox/Transforms/LoopBodyCloner.hpp"

#include "private/PassCommandLineOptions.hpp"

#include "private/PDGUtils.hpp"

#include "private/ITRUtils.hpp"

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

#include "llvm/ADT/SmallVector.h"
// using llvm::SmallVector

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

static llvm::cl::opt<SelectionStrategy> SelectionStrategyOption(
    "atrox-selection-strategy", llvm::cl::desc("block selection strategy"),
    llvm::cl::values(clEnumValN(SelectionStrategy::Naive, "naive", "naive"),
                     clEnumValN(SelectionStrategy::IteratorRecognitionBased,
                                "itr", "iterator recognition based")),
    llvm::cl::cat(AtroxCLCategory));

static void checkAndSetCmdLineOptions() {
  if (!SelectionStrategyOption.getPosition()) {
    SelectionStrategyOption.setValue(SelectionStrategy::Naive);
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

bool LoopBodyClonerPass::perform(
    llvm::Module &M,
    std::function<llvm::MemoryDependenceResults &(llvm::Function &)> &GetMDR) {
  bool hasChanged = false;

  llvm::SmallVector<llvm::Function *, 32> workList;
  workList.reserve(M.size());

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

    workList.push_back(&func);
  }

  while (!workList.empty()) {
    auto &func = *workList.pop_back_val();

    LoopBodyCloner lpc{M};

    // TODO consider obtaining these from pass manager and
    // preserving/invalidating them appropriately
    llvm::LoopInfo li{llvm::DominatorTree(const_cast<llvm::Function &>(func))};

    auto info = BuildITRInfo(li, *BuildPDG(func, &GetMDR(func)));

    if (SelectionStrategyOption ==
        SelectionStrategy::IteratorRecognitionBased) {
      // IteratorRecognitionSelector s{func, li, &GetMDR(func)};
      IteratorRecognitionSelector s{std::move(info)};
      hasChanged |= lpc.cloneLoops(li, s);
    } else {
      NaiveSelector s;
      hasChanged |= lpc.cloneLoops(li, s);
    }
  }

  return hasChanged;
}

llvm::PreservedAnalyses
LoopBodyClonerPass::run(llvm::Module &M, llvm::ModuleAnalysisManager &MAM) {
  auto &FAM =
      MAM.getResult<llvm::FunctionAnalysisManagerModuleProxy>(M).getManager();

  std::function<llvm::MemoryDependenceResults &(llvm::Function &)> GetMDR =
      [&](llvm::Function &F) -> llvm::MemoryDependenceResults & {
    return FAM.getResult<llvm::MemoryDependenceAnalysis>(F);
  };

  bool hasChanged = perform(M, GetMDR);

  return hasChanged ? llvm::PreservedAnalyses::all()
                    : llvm::PreservedAnalyses::none();
}

// legacy passmanager pass

void LoopBodyClonerLegacyPass::getAnalysisUsage(llvm::AnalysisUsage &AU) const {
  AU.addRequired<llvm::MemoryDependenceWrapperPass>();
  AU.setPreservesAll();
}

bool LoopBodyClonerLegacyPass::runOnModule(llvm::Module &M) {
  LoopBodyClonerPass pass;

  std::function<llvm::MemoryDependenceResults &(llvm::Function &)> GetMDR =
      [this](llvm::Function &F) -> llvm::MemoryDependenceResults & {
    return this->getAnalysis<MemoryDependenceWrapperPass>(F).getMemDep();
  };

  return pass.perform(M, GetMDR);
}

} // namespace atrox
