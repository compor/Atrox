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

#include "llvm/Support/Debug.h"
// using DEBUG macro
// using llvm::dbgs

#define DEBUG_TYPE "atrox-argutils"

namespace atrox {

bool ReorderInputs(llvm::SetVector<llvm::Value *> &Inputs,
                   const iteratorrecognition::IteratorInfo &Info) {
  bool changed = false;

  if (!Inputs.size()) {
    return changed;
  }

  auto found = std::find_if(Inputs.begin(), Inputs.end(), [&Info](auto *e) {
    if (const auto *i = llvm::dyn_cast<const llvm::Instruction>(e)) {
      LLVM_DEBUG(llvm::dbgs()
                     << "checking if input : " << *i << " is iterator\n";);
      return Info.isIterator(i);
    }

    return false;
  });

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

void GenerateArgDirection(const llvm::SetVector<llvm::Value *> &Inputs,
                          const llvm::SetVector<llvm::Value *> &Outputs,
                          llvm::SmallVectorImpl<ArgDirection> &ArgDirs,
                          MemoryAccessInfo *MAI) {

  for (auto *v : Inputs) {
    if (!v->getType()->isPointerTy()) {
      ArgDirs.push_back(ArgDirection::AD_Inbound);
      continue;
    }

    if (MAI && v->getType()->isPointerTy()) {
      ArgDirection dir = ArgDirection::AD_Inbound;

      if (MAI->isWrite(v)) {
        dir = ArgDirection::AD_Outbound;
        // ArgDirs.push_back(ArgDirection::AD_Both);
        // continue;

        if (MAI->isRead(v)) {
          dir = ArgDirection::AD_Both;
        }
      }

      // if (MAI->isRead(v)) {
      // dir |= ArgDirection::AD_Inbound;
      // ArgDirs.push_back(ArgDirection::AD_Inbound);
      //}

      ArgDirs.push_back(dir);
    } else {
      ArgDirs.push_back(ArgDirection::AD_Both);
    }
  }

  for (auto *v : Outputs) {
    if (!MAI) {
      ArgDirs.push_back(ArgDirection::AD_Both);
    } else {
      if (MAI->isRead(v)) {
        ArgDirs.push_back(ArgDirection::AD_Both);
        continue;
      }

      ArgDirs.push_back(ArgDirection::AD_Outbound);
    }
  }
}

} // namespace atrox

