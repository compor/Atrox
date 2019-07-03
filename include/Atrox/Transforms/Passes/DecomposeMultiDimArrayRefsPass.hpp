//
//
//

#pragma once

#include "Atrox/Config.hpp"

#include "Atrox/Transforms/DecomposeMultiDimArrayRefs.hpp"

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

#define ATROX_DECOMPOSEARRAYREFS_PASS_NAME "atrox-decompose-array-refs-pass"

namespace atrox {

// new passmanager pass
class DecomposeMultiDimArrayRefsPass
    : public llvm::PassInfoMixin<DecomposeMultiDimArrayRefsPass> {
public:
  DecomposeMultiDimArrayRefsPass();

  bool perform(llvm::Function &F);

  llvm::PreservedAnalyses run(llvm::Function &F,
                              llvm::FunctionAnalysisManager &FAM);
};

// legacy passmanager pass
class DecomposeMultiDimArrayRefsLegacyPass : public llvm::FunctionPass {
public:
  static char ID;

  void getAnalysisUsage(llvm::AnalysisUsage &AU) const override;
  DecomposeMultiDimArrayRefsLegacyPass() : llvm::FunctionPass(ID) {}

  bool runOnFunction(llvm::Function &F) override;
};

} // namespace atrox

