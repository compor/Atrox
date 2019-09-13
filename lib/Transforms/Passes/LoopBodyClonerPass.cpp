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

#include <fstream>
// using std::ifstream

#include <string>
// using std::string
// using std::to_string

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

static llvm::cl::opt<bool>
    ExportResults("atrox-export-results",
                  llvm::cl::desc("export results on successful extractions"),
                  llvm::cl::cat(AtroxCLCategory));

static llvm::cl::opt<bool>
    ExportFailResults("atrox-export-fail-results",
                      llvm::cl::desc("export results on failed extractions"),
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
  llvm::SmallVector<llvm::Function *, 32> workList;
  workList.reserve(M.size());

  if (ExportResults || ExportFailResults) {
    auto dirOrErr = iteratorrecognition::CreateDirectory(AtroxReportsDir);
    if (std::error_code ec = dirOrErr.getError()) {
      llvm::errs() << "Error: " << ec.message() << '\n';
      llvm::report_fatal_error("Failed to create reports directory" +
                               AtroxReportsDir);
    }

    AtroxReportsDir = dirOrErr.get();
  }

  auto not_in = [](const auto &C, const auto &E) {
    return C.end() == std::find(std::begin(C), std::end(C), E);
  };

  llvm::SmallVector<std::string, 32> FunctionWhiteList;

  if (AtroxFunctionWhiteListFile.getPosition()) {
    std::ifstream wlFile{AtroxFunctionWhiteListFile};

    std::string funcName;
    while (wlFile >> funcName) {
      FunctionWhiteList.push_back(funcName);
    }
  }

  for (auto &F : M) {
    if (F.isDeclaration() ||
        (AtroxFunctionWhiteListFile.getPosition() &&
         not_in(FunctionWhiteList, std::string{F.getName()}))) {
      continue;
    }

    workList.push_back(&F);
  }

  bool hasChanged = false;
  while (!workList.empty()) {
    auto &F = *workList.pop_back_val();

    LLVM_DEBUG(llvm::dbgs() << "processing func: " << F.getName() << '\n';);

    LoopBodyCloner lpc{M, ExportResults, ExportFailResults};

    // TODO consider obtaining these from pass manager and
    // preserving/invalidating them appropriately
    auto dt = llvm::DominatorTree(const_cast<llvm::Function &>(F));
    llvm::LoopInfo li{dt};

    auto itrInfo = BuildITRInfo(li, *BuildPDG(F, &GetMDR(F)));
    auto &SE = GetSE(F);
    auto &AA = GetAA(F);

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

    if (ExportResults || ExportFailResults) {
      unsigned int successCnt = 0, failCnt = 0;
      auto &i = lpc.getInfo();

      for (auto &e : i) {
        if (e.Func) {
          WriteJSONToFile(llvm::json::toJSON(e),
                          "lpc." + F.getName() + ".extracted." +
                              std::to_string(successCnt),
                          AtroxReportsDir);
          successCnt++;
        } else {
          WriteJSONToFile(llvm::json::toJSON(e),
                          "lpc." + F.getName() + ".unextracted." +
                              std::to_string(failCnt),
                          AtroxReportsDir);
          failCnt++;
        }
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
  AU.addRequiredTransitive<llvm::ScalarEvolutionWrapperPass>();
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
