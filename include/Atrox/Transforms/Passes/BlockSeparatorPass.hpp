
//
//
//

#pragma once

#include "Atrox/Config.hpp"

#include "Atrox/Transforms/BlockSeparator.hpp"

#include "llvm/Pass.h"
// using llvm::FunctionPass

#include "llvm/IR/PassManager.h"
// using llvm::FunctionAnalysisManager
// using llvm::PassInfoMixin

#include <functional>
// using std::function

namespace llvm {
class Function;
} // namespace llvm

#define ATROX_BLOCKSEPARATOR_PASS_NAME "atrox-block-separator"

namespace atrox {

// new passmanager pass
class BlockSeparatorPass : public llvm::PassInfoMixin<BlockSeparatorPass> {
public:
  BlockSeparatorPass();

  bool perform(llvm::Function &F);

  llvm::PreservedAnalyses run(llvm::Function &F,
                              llvm::FunctionAnalysisManager &FAM);
};

// legacy passmanager pass
class BlockSeparatorLegacyPass : public llvm::FunctionPass {
public:
  static char ID;

  void getAnalysisUsage(llvm::AnalysisUsage &AU) const override;
  BlockSeparatorLegacyPass() : llvm::FunctionPass(ID) {}

  bool runOnFunction(llvm::Function &F) override;
};

} // namespace atrox

