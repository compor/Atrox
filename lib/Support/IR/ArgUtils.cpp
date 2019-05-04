//
//
//

#include "Atrox/Support/IR/ArgUtils.hpp"

#include "Atrox/Support/IR/ArgDirection.hpp"

#include "IteratorRecognition/Analysis/IteratorValueTracking.hpp"

#include "llvm/IR/Value.h"
// using llvm::Value

#include "llvm/ADT/SetVector.h"
// using llvm::SetVector

namespace atrox {

void generateArgIteratorVariance(
    const llvm::SetVector<llvm::Value *> &Inputs,
    const llvm::SetVector<llvm::Value *> &Outputs,
    const iteratorrecognition::IteratorInfo &Info,
    llvm::SmallVector<bool, 16> &ArgIteratorVariance) {
  iteratorrecognition::IteratorDispositionAnalyzer ida{Info};

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

} // namespace atrox

