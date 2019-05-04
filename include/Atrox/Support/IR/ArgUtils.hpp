//
//
//

#pragma once

#include "Atrox/Config.hpp"

#include "IteratorRecognition/Analysis/IteratorRecognition.hpp"

#include "llvm/ADT/SetVector.h"
// using llvm::SetVector

namespace llvm {
class Value;
} // namespace llvm

namespace atrox {

void generateArgIteratorVariance(
    const llvm::SetVector<llvm::Value *> &Inputs,
    const llvm::SetVector<llvm::Value *> &Outputs,
    const iteratorrecognition::IteratorInfo &Info,
    llvm::SmallVector<bool, 16> &ArgIteratorVariance);

} // namespace atrox

