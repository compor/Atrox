//
//
//

#include "Atrox/Config.hpp"

#include "Atrox/Util.hpp"

#include "Atrox/Debug.hpp"

#include "Atrox/Transforms/Passes/BlockSeparatorPass.hpp"

#include "Atrox/Transforms/BlockSeparator.hpp"

#include "private/PDGUtils.hpp"

#include "private/ITRUtils.hpp"

#include "private/PassCommandLineOptions.hpp"

#include "llvm/Pass.h"
// using llvm::RegisterPass

#include "llvm/IR/Instruction.h"
// using llvm::Instruction

#include "llvm/IR/Function.h"
// using llvm::Function

#include "llvm/IR/LegacyPassManager.h"
// using llvm::PassManagerBase

#include "llvm/IR/Dominators.h"
// using llvm::DominatorTree

#include "llvm/Analysis/LoopInfo.h"
// using llvm::LoopInfo

#include "llvm/Analysis/MemoryDependenceAnalysis.h"
// using llvm::MemoryDependenceResults

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

bool BlockSeparatorPass::perform(llvm::Function &F, llvm::DominatorTree *DT,
                                 llvm::LoopInfo *LI,
                                 llvm::MemoryDependenceResults *MDR) {
  auto not_in = [](const auto &C, const auto &E) {
    return C.end() == std::find(std::begin(C), std::end(C), E);
  };

  if (F.isDeclaration() ||
      (AtroxFunctionWhiteList.size() &&
       not_in(AtroxFunctionWhiteList, std::string{F.getName()}))) {
    return false;
  }

  LLVM_DEBUG(llvm::dbgs() << "processing func: " << F.getName() << '\n';);

  auto itrInfo = BuildITRInfo(*LI, *BuildPDG(F, MDR));

  // NOTE
  // this does not update the iterator info with the uncond branch instruction
  // that might be added by block splitting
  // however that instruction can acquire the mode of its immediately
  // preceding instruction

  bool hasChanged = false;
  for (auto *curLoop : LI->getLoopsInPreorder()) {
    LLVM_DEBUG(llvm::dbgs() << "processing loop with header: "
                            << curLoop->getHeader()->getName() << '\n';);

    BlockModeChangePointMapTy modeChanges;
    BlockModeMapTy blockModes;

    auto infoOrError = itrInfo->getIteratorInfoFor(curLoop);

    if (!infoOrError) {
      continue;
    }
    auto &info = *infoOrError;

    bool found = FindPartitionPoints(*curLoop, info, blockModes, modeChanges);
    LLVM_DEBUG(llvm::dbgs() << "partition points found: " << modeChanges.size()
                            << '\n';);

    if (found) {
      SplitAtPartitionPoints(modeChanges, blockModes, DT, LI);
      hasChanged = true;
    }
  }

  return hasChanged;
}

llvm::PreservedAnalyses
BlockSeparatorPass::run(llvm::Function &F, llvm::FunctionAnalysisManager &FAM) {

  auto *DT = &FAM.getResult<llvm::DominatorTreeAnalysis>(F);
  auto *LI = &FAM.getResult<llvm::LoopAnalysis>(F);
  auto &MDR = FAM.getResult<llvm::MemoryDependenceAnalysis>(F);

  bool hasChanged = perform(F, DT, LI, &MDR);

  if (!hasChanged) {
    return llvm::PreservedAnalyses::all();
  }

  llvm::PreservedAnalyses PA;
  PA.preserve<llvm::DominatorTreeAnalysis>();
  PA.preserve<llvm::LoopAnalysis>();

  return PA;
}

// legacy passmanager pass

void BlockSeparatorLegacyPass::getAnalysisUsage(llvm::AnalysisUsage &AU) const {
  AU.addRequired<llvm::DominatorTreeWrapperPass>();
  AU.addRequired<llvm::LoopInfoWrapperPass>();
  AU.addRequired<llvm::MemoryDependenceWrapperPass>();

  AU.addPreserved<llvm::DominatorTreeWrapperPass>();
  AU.addPreserved<llvm::LoopInfoWrapperPass>();
}

bool BlockSeparatorLegacyPass::runOnFunction(llvm::Function &F) {
  BlockSeparatorPass pass;

  auto *DT = &getAnalysis<llvm::DominatorTreeWrapperPass>().getDomTree();
  auto *LI = &getAnalysis<llvm::LoopInfoWrapperPass>().getLoopInfo();
  auto &MDR = getAnalysis<llvm::MemoryDependenceWrapperPass>().getMemDep();

  return pass.perform(F, DT, LI, &MDR);
}

} // namespace atrox

