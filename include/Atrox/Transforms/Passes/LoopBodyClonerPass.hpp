//
//
//

#pragma once

#include "Atrox/Config.hpp"

#include "llvm/Pass.h"
// using llvm::ModulePass

#include "llvm/Analysis/ScalarEvolution.h"
// using llvm::ScalarEvolution

#include "llvm/Analysis/MemoryDependenceAnalysis.h"
// using llvm::MemoryDependenceResults

#include "llvm/Analysis/AliasAnalysis.h"
// using llvm::AAResults

#include "llvm/IR/PassManager.h"
// using llvm::ModuleAnalysisManager
// using llvm::PassInfoMixin

#include <functional>
// using std::function

namespace llvm {
class Module;
} // namespace llvm

#define ATROX_LOOPBODYCLONER_PASS_NAME "atrox-lbc-pass"

namespace atrox {

// new passmanager pass
class LoopBodyClonerPass : public llvm::PassInfoMixin<LoopBodyClonerPass> {
public:
  LoopBodyClonerPass();

  bool perform(
      llvm::Module &M,
      std::function<llvm::ScalarEvolution &(llvm::Function &)> &GetSE,
      std::function<llvm::MemoryDependenceResults &(llvm::Function &)> &GetMDR,
      std::function<llvm::AAResults &(llvm::Function &)> &GetAA);

  llvm::PreservedAnalyses run(llvm::Module &M,
                              llvm::ModuleAnalysisManager &MAM);
};

// legacy passmanager pass
class LoopBodyClonerLegacyPass : public llvm::ModulePass {
public:
  static char ID;

  void getAnalysisUsage(llvm::AnalysisUsage &AU) const override;
  LoopBodyClonerLegacyPass() : llvm::ModulePass(ID) {}

  bool runOnModule(llvm::Module &M) override;
};

} // namespace atrox

