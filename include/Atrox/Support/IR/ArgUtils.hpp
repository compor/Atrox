//
//
//

#pragma once

#include "Atrox/Config.hpp"

#include "Atrox/Support/IR/ArgDirection.hpp"

#include "Atrox/Analysis/MemoryAccessInfo.hpp"

#include "IteratorRecognition/Analysis/IteratorRecognition.hpp"

#include "llvm/IR/ValueMap.h"
// using llvm::ValueMap

#include "llvm/ADT/SmallVector.h"
// using llvm::SmallVectorImpl

#include "llvm/ADT/SetVector.h"
// using llvm::SetVector

namespace llvm {
class Value;
} // namespace llvm

namespace atrox {

inline bool IsBidirectional(
    const llvm::Value *V,
    const llvm::ValueMap<llvm::Value *, llvm::Value *> &OutputToInput) {
  for (const auto &e : OutputToInput) {
    if (e.first == V || e.second == V) {
      return true;
    }
  }
  return false;
}

void GenerateArgIteratorVariance(
    const llvm::SetVector<llvm::Value *> &Inputs,
    const llvm::SetVector<llvm::Value *> &Outputs,
    const iteratorrecognition::IteratorInfo &Info,
    llvm::SmallVectorImpl<bool> &ArgIteratorVariance);

void GenerateArgDirection(
    const llvm::SetVector<llvm::Value *> &Inputs,
    const llvm::SetVector<llvm::Value *> &Outputs,
    const llvm::ValueMap<llvm::Value *, llvm::Value *> &OutputToInput,
    llvm::SmallVectorImpl<ArgDirection> &ArgDirs,
    MemoryAccessInfo *MAI = nullptr);

} // namespace atrox

