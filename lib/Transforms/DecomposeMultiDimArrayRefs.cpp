//
//
//

#include "Atrox/Transforms/DecomposeMultiDimArrayRefs.hpp"

#include "llvm/IR/Value.h"
// using llvm::Value

#include "llvm/IR/Instruction.h"
// using llvm::Instruction

#include "llvm/IR/Instructions.h"
// using llvm::CastInst
// using llvm::GetElementPtrInst

#include "llvm/IR/Instructions.h"
// using llvm::Type
// using llvm::ArrayType
// using llvm::PointerType

#include "llvm/Support/Debug.h"
// using LLVM_DEBUG macro
// using llvm::dbgs
// using llvm::errs

#include <vector>
// using std::vector

#define DEBUG_TYPE "atrox-decompose-array-refs"

namespace atrox {

llvm::Type *
GetDecomposedMultiDimArrayType(const llvm::PointerType *PtrTy,
                               llvm::SmallVectorImpl<llvm::Type *> &NewTypes) {
  if (!PtrTy) {
    return nullptr;
  }

  unsigned levels = 0;

  auto *elemTy = PtrTy->getElementType();
  levels++;

  llvm::Type *basicTy = nullptr;
  while (auto *arrayTy = llvm::dyn_cast<llvm::ArrayType>(elemTy)) {
    basicTy = elemTy = arrayTy->getElementType();
    levels++;
  }

  LLVM_DEBUG(llvm::dbgs() << "basic type: " << *basicTy << '\n'
                          << "indirection levels: " << levels << '\n';);

  auto *ptrTy = llvm::PointerType::getUnqual(basicTy);
  NewTypes.push_back(ptrTy);

  for (auto i = 1u; i < levels; ++i) {
    NewTypes.push_back(llvm::PointerType::getUnqual(NewTypes.back()));
  }

  std::reverse(NewTypes.begin(), NewTypes.end());

  return basicTy;
}

bool DecomposeMultiDimArrayRefs(llvm::GetElementPtrInst *GEP) {
  assert(GEP && "GEP instruction is null!");

  LLVM_DEBUG(llvm::dbgs() << "decomposing: " << *GEP << '\n';);

  if (GEP->getNumIndices() < 2) {
    LLVM_DEBUG(llvm::dbgs() << "GEP instruction already has 1 index\n";);
    return false;
  }

  llvm::Value *lastPtr = GEP->getPointerOperand();
  llvm::Instruction *insertPoint = GEP;
  llvm::SmallVector<llvm::Type *, 8> newTypes;

  GetDecomposedMultiDimArrayType(
      llvm::dyn_cast<llvm::PointerType>(GEP->getPointerOperandType()),
      newTypes);

  if (newTypes.size() != GEP->getNumIndices()) {
    // TODO check the relation with other composite type e.g. structs
    LLVM_DEBUG(llvm::dbgs() << "GEP instruction does not step through arrays "
                               "in all its indices\n";);
    return false;
  }

  lastPtr = llvm::CastInst::CreatePointerCast(lastPtr, newTypes[0], "ptrcast",
                                              insertPoint);

  // this uses Value because the gep pointer may be a global variable
  std::vector<llvm::Value *> newInsts;
  newInsts.push_back(lastPtr);
  std::vector<llvm::Value *> indices{GEP->idx_begin(), GEP->idx_end()};

  for (auto i = 0u; i < indices.size(); ++i) {
    auto *elemTy =
        llvm::dyn_cast<llvm::PointerType>(newTypes[i])->getElementType();
    lastPtr = llvm::GetElementPtrInst::Create(elemTy, lastPtr, indices[i],
                                              "lastptr", insertPoint);
    newInsts.push_back(lastPtr);

    // do not load the contents of the last ptr,
    // since we are replacing its uses later
    if (i < indices.size() - 1) {
      lastPtr = new llvm::LoadInst(lastPtr, "ptrload", insertPoint);
      newInsts.push_back(lastPtr);
    }
  }

  LLVM_DEBUG(for (auto *e : newInsts) { llvm::dbgs() << *e << '\n'; };);

  if (auto *dbg = GEP->getMetadata("dbg")) {
    for (auto *e : newInsts) {
      if (auto *i = llvm::dyn_cast<llvm::Instruction>(e)) {
        i->setMetadata("dbg", dbg);
      }
    }
  }

  GEP->replaceAllUsesWith(newInsts.back());
  GEP->eraseFromParent();

  return true;
}

} // namespace atrox

