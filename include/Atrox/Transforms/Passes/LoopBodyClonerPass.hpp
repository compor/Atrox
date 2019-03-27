//
//
//

#pragma once

#include "Atrox/Config.hpp"

#include "llvm/Pass.h"
// using llvm::ModulePass

#include "llvm/IR/PassManager.h"
// using llvm::ModuleAnalysisManager
// using llvm::PassInfoMixin

namespace llvm {
class Module;
} // namespace llvm

#define ATROX_LOOPBODYCLONER_PASS_NAME "atrox-loop-body-clone"

namespace atrox {

// new passmanager pass
class LoopBodyClonerPass : public llvm::PassInfoMixin<LoopBodyClonerPass> {
public:
  LoopBodyClonerPass();

  bool run(llvm::Module &F);
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

