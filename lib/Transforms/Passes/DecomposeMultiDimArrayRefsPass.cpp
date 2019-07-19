//
//
//

#include "Atrox/Config.hpp"

#include "Atrox/Util.hpp"

#include "Atrox/Debug.hpp"

#include "Atrox/Transforms/Passes/DecomposeMultiDimArrayRefsPass.hpp"

#include "Atrox/Transforms/DecomposeMultiDimArrayRefs.hpp"

#include "Atrox/Support/MemAccInst.hpp"

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

#define DEBUG_TYPE ATROX_DECOMPOSEARRAYREFS_PASS_NAME
#define PASS_CMDLINE_OPTIONS_ENVVAR "DECOMPOSEARRAYREFS_CMDLINE_OPTIONS"

// plugin registration for opt

char atrox::DecomposeMultiDimArrayRefsLegacyPass::ID = 0;

static llvm::RegisterPass<atrox::DecomposeMultiDimArrayRefsLegacyPass>
    X(DEBUG_TYPE, PRJ_CMDLINE_DESC("loop body clone pass"), false, false);

// plugin registration for clang

// the solution was at the bottom of the header file
// 'llvm/Transforms/IPO/PassManagerBuilder.h'
// create a static free-floating callback that uses the legacy pass manager to
// add an instance of this pass and a static instance of the
// RegisterStandardPasses class

static void registerDecomposeMultiDimArrayRefsLegacyPass(
    const llvm::PassManagerBuilder &Builder,
    llvm::legacy::PassManagerBase &PM) {
  PM.add(new atrox::DecomposeMultiDimArrayRefsLegacyPass());

  return;
}

static llvm::RegisterStandardPasses
    RegisterDecomposeMultiDimArrayRefsLegacyPass(
        llvm::PassManagerBuilder::EP_EarlyAsPossible,
        registerDecomposeMultiDimArrayRefsLegacyPass);

//

namespace atrox {

// new passmanager pass

DecomposeMultiDimArrayRefsPass::DecomposeMultiDimArrayRefsPass() {
  llvm::cl::ResetAllOptionOccurrences();
  llvm::cl::ParseEnvironmentOptions(DEBUG_TYPE, PASS_CMDLINE_OPTIONS_ENVVAR);
}

bool DecomposeMultiDimArrayRefsPass::perform(llvm::Function &F) {
  auto not_in = [](const auto &C, const auto &E) {
    return C.end() == std::find(std::begin(C), std::end(C), E);
  };

  if (F.isDeclaration() ||
      (AtroxFunctionWhiteList.size() &&
       not_in(AtroxFunctionWhiteList, std::string{F.getName()}))) {
    return false;
  }

  LLVM_DEBUG(llvm::dbgs() << "processing func: " << F.getName() << '\n';);

  bool hasChanged = false;
  MemAccInstVisitor accesses;
  accesses.visit(F);

  for (auto &e : accesses.Accesses) {
    llvm::SmallVector<llvm::GetElementPtrInst *, 8> geps;
    if (auto *gep = llvm::dyn_cast_or_null<llvm::GetElementPtrInst>(
            e.getPointerOperand())) {
      geps.push_back(gep);
    }

    auto last = std::unique(geps.begin(), geps.end());
    geps.erase(last, geps.end());

    for (auto *gep : geps) {
      hasChanged |= DecomposeMultiDimArrayRefs(gep);
    }
  }

  return hasChanged;
}

llvm::PreservedAnalyses
DecomposeMultiDimArrayRefsPass::run(llvm::Function &F,
                                    llvm::FunctionAnalysisManager &FAM) {
  bool hasChanged = perform(F);

  if (!hasChanged) {
    return llvm::PreservedAnalyses::all();
  }

  llvm::PreservedAnalyses PA;
  PA.preserveSet<llvm::CFGAnalyses>();

  return PA;
}

// legacy passmanager pass

void DecomposeMultiDimArrayRefsLegacyPass::getAnalysisUsage(
    llvm::AnalysisUsage &AU) const {
  AU.setPreservesCFG();
}

bool DecomposeMultiDimArrayRefsLegacyPass::runOnFunction(llvm::Function &F) {
  DecomposeMultiDimArrayRefsPass pass;

  return pass.perform(F);
}

} // namespace atrox
