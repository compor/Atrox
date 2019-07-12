//
//
//

#include "Atrox/Config.hpp"

#include "Atrox/Util.hpp"

#include "Atrox/Debug.hpp"

#include "Atrox/Analysis/NaiveSelector.hpp"

#include "Atrox/Analysis/IteratorRecognitionSelector.hpp"

#include "Atrox/Analysis/WeightedIteratorRecognitionSelector.hpp"

#include "Atrox/Transforms/Passes/LoopBodyClonerPass.hpp"

#include "Atrox/Transforms/LoopBodyCloner.hpp"

#include "Atrox/Transforms/BlockSeparator.hpp"

#include "Atrox/Exchange/JSONTransfer.hpp"

// TODO maybe factor out this code to common utility project
#include "IteratorRecognition/Support/FileSystem.hpp"

#include "private/PassCommandLineOptions.hpp"

#include "private/PDGUtils.hpp"

#include "private/ITRUtils.hpp"

#include "llvm/Pass.h"
// using llvm::RegisterPass

#include "llvm/Analysis/ScalarEvolution.h"
// using llvm::ScalarEvolutionWrapperPass
// using llvm::ScalarEvolution

#include "llvm/Analysis/AliasAnalysis.h"
// using llvm::AAResultsWrapperPass
// using llvm::AAResults

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

enum class SelectionStrategy {
  Naive,
  IteratorRecognitionBased,
  WeightedIteratorRecognitionBased
};

static llvm::cl::opt<SelectionStrategy> SelectionStrategyOption(
    "atrox-selection-strategy", llvm::cl::desc("block selection strategy"),
    llvm::cl::values(
        clEnumValN(SelectionStrategy::Naive, "naive", "naive"),
        clEnumValN(SelectionStrategy::IteratorRecognitionBased, "itr",
                   "iterator recognition based"),
        clEnumValN(SelectionStrategy::WeightedIteratorRecognitionBased, "witr",
                   "weighted iterator recognition based")),
    llvm::cl::cat(AtroxCLCategory));

static llvm::cl::opt<bool> SeparateBlocksOption(
    "atrox-separate-blocks",
    llvm::cl::desc("separate blocks based on iterator recognition"),
    llvm::cl::init(true), llvm::cl::Hidden, llvm::cl::cat(AtroxCLCategory));

static llvm::cl::opt<bool> ExportResults("atrox-export-results",
                                         llvm::cl::desc("export results"),
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
    std::function<llvm::ScalarEvolution &(llvm::Function &)> &GetSE,
    std::function<llvm::MemoryDependenceResults &(llvm::Function &)> &GetMDR,
    std::function<llvm::AAResults &(llvm::Function &)> &GetAA) {
  bool hasChanged = false;

  llvm::SmallVector<llvm::Function *, 32> workList;
  workList.reserve(M.size());

  for (auto &func : M) {
    if (func.isDeclaration()) {
      continue;
    }

    if (AtroxFunctionWhiteList.size()) {
      auto found =
          std::find(AtroxFunctionWhiteList.begin(),
                    AtroxFunctionWhiteList.end(), std::string{func.getName()});

      if (found == AtroxFunctionWhiteList.end()) {
        continue;
      }
    }

    workList.push_back(&func);
  }

  if (ExportResults) {
    auto dirOrErr = iteratorrecognition::CreateDirectory(AtroxReportsDir);
    if (std::error_code ec = dirOrErr.getError()) {
      llvm::errs() << "Error: " << ec.message() << '\n';
      llvm::report_fatal_error("Failed to create reports directory" +
                               AtroxReportsDir);
    }

    AtroxReportsDir = dirOrErr.get();
  }

  while (!workList.empty()) {
    auto &func = *workList.pop_back_val();

    LoopBodyCloner lpc{M, ExportResults};

    // TODO consider obtaining these from pass manager and
    // preserving/invalidating them appropriately
    auto dt = llvm::DominatorTree(const_cast<llvm::Function &>(func));
    llvm::LoopInfo li{dt};

    auto itrInfo = BuildITRInfo(li, *BuildPDG(func, &GetMDR(func)));
    auto &SE = GetSE(func);
    auto &AA = GetAA(func);

    // NOTE
    // this does not update the iterator info with the uncond branch instruction
    // that might be added by block splitting
    // however that instruction can acquire the mode of its immediately
    // preceding instruction
    if (SeparateBlocksOption) {
      auto loops = li.getLoopsInPreorder();

      for (auto *curLoop : loops) {
        BlockModeChangePointMapTy modeChanges;
        BlockModeMapTy blockModes;

        auto infoOrError = itrInfo->getIteratorInfoFor(curLoop);

        if (!infoOrError) {
          continue;
        }
        auto &info = *infoOrError;

        bool found =
            FindPartitionPoints(*curLoop, info, blockModes, modeChanges);

        if (found) {
          SplitAtPartitionPoints(modeChanges, blockModes, &dt, &li);
          hasChanged = true;
          LLVM_DEBUG(llvm::dbgs() << "partition points found: "
                                  << modeChanges.size() << '\n';);
        } else {
          LLVM_DEBUG(llvm::dbgs() << "No partition points found\n";);
        }
      }
    }

    if (SelectionStrategyOption ==
        SelectionStrategy::IteratorRecognitionBased) {
      IteratorRecognitionSelector s{*itrInfo};
      hasChanged |= lpc.cloneLoops(li, s, &*itrInfo, &SE, &AA);
    } else if (SelectionStrategyOption ==
               SelectionStrategy::WeightedIteratorRecognitionBased) {
      WeightedIteratorRecognitionSelector s{*itrInfo};
      hasChanged |= lpc.cloneLoops(li, s, &*itrInfo, &SE, &AA);
    } else {
      NaiveSelector s;
      hasChanged |= lpc.cloneLoops(li, s, &*itrInfo, &SE, &AA);
    }

    if (hasChanged && ExportResults) {
      auto &i = lpc.getInfo();

      for (auto &e : i) {
        WriteJSONToFile(llvm::json::toJSON(e), "lpc." + e.Func->getName(),
                        AtroxReportsDir);
      }
    }
  }

  return hasChanged;
}

llvm::PreservedAnalyses
LoopBodyClonerPass::run(llvm::Module &M, llvm::ModuleAnalysisManager &MAM) {
  auto &FAM =
      MAM.getResult<llvm::FunctionAnalysisManagerModuleProxy>(M).getManager();

  std::function<llvm::ScalarEvolution &(llvm::Function &)> GetSE =
      [&](llvm::Function &F) -> llvm::ScalarEvolution & {
    return FAM.getResult<llvm::ScalarEvolutionAnalysis>(F);
  };

  std::function<llvm::MemoryDependenceResults &(llvm::Function &)> GetMDR =
      [&](llvm::Function &F) -> llvm::MemoryDependenceResults & {
    return FAM.getResult<llvm::MemoryDependenceAnalysis>(F);
  };

  std::function<llvm::AAResults &(llvm::Function &)> GetAA =
      [&](llvm::Function &F) -> llvm::AAResults & {
    return FAM.getResult<llvm::AAManager>(F);
  };

  bool hasChanged = perform(M, GetSE, GetMDR, GetAA);

  return hasChanged ? llvm::PreservedAnalyses::all()
                    : llvm::PreservedAnalyses::none();
}

// legacy passmanager pass

void LoopBodyClonerLegacyPass::getAnalysisUsage(llvm::AnalysisUsage &AU) const {
  AU.addRequired<llvm::ScalarEvolutionWrapperPass>();
  AU.addRequiredTransitive<llvm::AAResultsWrapperPass>();
  AU.addRequired<llvm::MemoryDependenceWrapperPass>();
  AU.setPreservesAll();
}

bool LoopBodyClonerLegacyPass::runOnModule(llvm::Module &M) {
  LoopBodyClonerPass pass;

  std::function<llvm::ScalarEvolution &(llvm::Function &)> GetSE =
      [this](llvm::Function &F) -> llvm::ScalarEvolution & {
    return this->getAnalysis<llvm::ScalarEvolutionWrapperPass>(F).getSE();
  };

  std::function<llvm::MemoryDependenceResults &(llvm::Function &)> GetMDR =
      [this](llvm::Function &F) -> llvm::MemoryDependenceResults & {
    return this->getAnalysis<MemoryDependenceWrapperPass>(F).getMemDep();
  };

  std::function<llvm::AAResults &(llvm::Function &)> GetAA =
      [this](llvm::Function &F) -> llvm::AAResults & {
    return this->getAnalysis<AAResultsWrapperPass>(F).getAAResults();
  };

  return pass.perform(M, GetSE, GetMDR, GetAA);
}

} // namespace atrox
