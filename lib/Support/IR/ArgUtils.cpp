//
//
//

#include "Atrox/Support/IR/ArgUtils.hpp"

#include "llvm/IR/Value.h"
// using llvm::Value

#include "llvm/Support/Debug.h"
// using LLVM_DEBUG macro
// using llvm::dbgs

#include <algorithm>
// using std::find_if
// using std::reverse

#include <cassert>
// using assert

#define DEBUG_TYPE "atrox-argutils"

namespace atrox {

bool ReorderInputs(llvm::SetVector<llvm::Value *> &Inputs,
                   const iteratorrecognition::IteratorInfo &Info) {
  bool changed = false;

  auto found = std::find_if(Inputs.begin(), Inputs.end(), [&Info](auto *e) {
    if (const auto *i = llvm::dyn_cast<const llvm::Instruction>(e)) {
      return Info.isIterator(i);
    }

    return false;
  });

  assert(found != Inputs.end() &&
         "There must be at least one iterator variable!");

  if (found != Inputs.begin()) {
    LLVM_DEBUG(llvm::dbgs() << "reordering inputs\n";);
    auto *tmp = *found;
    Inputs.erase(found);

    auto rest = Inputs.takeVector();
    std::reverse(rest.begin(), rest.end());
    rest.push_back(tmp);
    std::reverse(rest.begin(), rest.end());

    Inputs.insert(rest.begin(), rest.end());

    changed = true;

    LLVM_DEBUG(for (auto *e : Inputs) { llvm::dbgs() << *e << '\n'; };);
  }

  return changed;
}

void GenerateArgIteratorVariance(
    const llvm::Loop &CurL, const llvm::SetVector<llvm::Value *> &Inputs,
    const llvm::SetVector<llvm::Value *> &Outputs,
    iteratorrecognition::DispositionTracker &IDT,
    llvm::SmallVectorImpl<bool> &ArgIteratorVariance) {
  for (auto *e : Inputs) {
    switch (static_cast<int>(IDT.getDisposition(e, &CurL, true))) {
    default:
      ArgIteratorVariance.push_back(false);
      break;
    case 2:
      ArgIteratorVariance.push_back(true);
      break;
    }
  }

  for (auto *e : Outputs) {
    switch (static_cast<int>(IDT.getDisposition(e, &CurL, true))) {
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
    llvm::SmallVectorImpl<ArgDirection> &ArgDirs, MemoryAccessInfo *MAI) {

  for (auto *v : Inputs) {
    if (IsBidirectional(v, OutputToInput)) {
      continue;
    }

    if (MAI && v->getType()->isPointerTy()) {
      if (MAI->isWrite(v)) {
        ArgDirs.push_back(ArgDirection::AD_Both);
        continue;
      }

      if (MAI->isRead(v)) {
        ArgDirs.push_back(ArgDirection::AD_Inbound);
      }
    } else {
      if (!IsBidirectional(v, OutputToInput)) {
        ArgDirs.push_back(ArgDirection::AD_Inbound);
      } else {
        ArgDirs.push_back(ArgDirection::AD_Both);
      }
    }
  }

  for (auto *v : Outputs) {
    ArgDirs.push_back(IsBidirectional(v, OutputToInput)
                          ? ArgDirection::AD_Both
                          : ArgDirection::AD_Outbound);
  }
}

} // namespace atrox

