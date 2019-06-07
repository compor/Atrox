//
//
//

#include "Atrox/Analysis/MemoryAccessInfo.hpp"

#include "llvm/Analysis/AliasAnalysis.h"
// using llvm::AAResults

#include "llvm/Analysis/MemoryLocation.h"
// using llvm::MemoryLocation

#include "llvm/ADT/SmallPtrSet.h"
// using llvm::SmallPtrSet

#include <algorithm>
// using std::find

namespace atrox {

void MemoryAccessInfo::filterMemoryAccesses() {
  for (auto *bb : Blocks) {
    for (auto &inst : *bb) {
      if (inst.mayReadFromMemory()) {
        Loads.push_back(&inst);
      }

      if (inst.mayWriteToMemory()) {
        Stores.push_back(&inst);
      }
    }
  }
}

void MemoryAccessInfo::findAccessPointers() {
  llvm::SmallPtrSet<llvm::Value *, 16> Seen;

  for (auto *st : Stores) {
    auto loc = llvm::MemoryLocation::get(st);
    auto *ptr = const_cast<llvm::Value *>(loc.Ptr);

    if (Seen.insert(ptr).second) {
      StorePtrs.push_back(ptr);
      AST.add(ptr, llvm::MemoryLocation::UnknownSize, loc.AATags);
    }
  }

  for (auto *ld : Loads) {
    auto loc = llvm::MemoryLocation::get(ld);
    auto *ptr = const_cast<llvm::Value *>(loc.Ptr);

    if (Seen.insert(ptr).second) {
      LoadPtrs.push_back(ptr);
      AST.add(ptr, llvm::MemoryLocation::UnknownSize, loc.AATags);
    }
  }
}

bool MemoryAccessInfo::isRead(llvm::Value *V) {
  return std::find(LoadPtrs.begin(), LoadPtrs.end(), V) != LoadPtrs.end();
}

bool MemoryAccessInfo::isWrite(llvm::Value *V) {
  return std::find(StorePtrs.begin(), StorePtrs.end(), V) != StorePtrs.end();
}

} // namespace atrox

