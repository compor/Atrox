//
//
//

#include "Atrox/Support/IR/ArgUtils.hpp"

#include "IteratorRecognition/Analysis/DispositionTracker.hpp"

#include "llvm/IR/Value.h"
// using llvm::Value

namespace atrox {

void GenerateArgIteratorVariance(
    const llvm::SetVector<llvm::Value *> &Inputs,
    const llvm::SetVector<llvm::Value *> &Outputs,
    const iteratorrecognition::IteratorInfo &Info,
    llvm::SmallVectorImpl<bool> &ArgIteratorVariance) {
  iteratorrecognition::DispositionTracker ida{Info};

  for (auto *e : Inputs) {
    switch (static_cast<int>(ida.getDisposition(e))) {
    default:
      ArgIteratorVariance.push_back(false);
      break;
    case 2:
      ArgIteratorVariance.push_back(true);
      break;
    }
  }

  for (auto *e : Outputs) {
    switch (static_cast<int>(ida.getDisposition(e))) {
    default:
      ArgIteratorVariance.push_back(false);
      break;
    case 2:
      ArgIteratorVariance.push_back(true);
      break;
    }
  }
}

void GenerateArgDirection(
    const llvm::SetVector<llvm::Value *> &Inputs,
    const llvm::SetVector<llvm::Value *> &Outputs,
    const llvm::ValueMap<llvm::Value *, llvm::Value *> &OutputToInput,
    llvm::SmallVectorImpl<ArgDirection> &ArgDirs, llvm::AAResults *AA) {

  for (auto *v : Inputs) {
    if (!IsBidirectional(v, OutputToInput)) {
      ArgDirs.push_back(ArgDirection::AD_Inbound);
    }
  }

  for (auto *v : Outputs) {
    ArgDirs.push_back(IsBidirectional(v, OutputToInput)
                          ? ArgDirection::AD_Both
                          : ArgDirection::AD_Outbound);
  }
}

} // namespace atrox

