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

#include "llvm/IR/IRBuilder.h"
// using llvm::IRBuilder

#include "llvm/Support/Debug.h"
// using LLVM_DEBUG macro
// using llvm::dbgs
// using llvm::errs

#include <vector>
// using std::vector

#include <iterator>
// using std::back_inserter

#include <algorithm>
// using std::copy

#define DEBUG_TYPE "atrox-decompose-array-refs"

namespace atrox {

llvm::Type *
GetDecomposedMultiDimArrayType(const llvm::PointerType *PtrTy,
                               llvm::SmallVectorImpl<llvm::Type *> *NewTypes) {
  if (!PtrTy) {
    return nullptr;
  }

  unsigned dim = 0;

  auto *elemTy = PtrTy->getElementType();
  dim++;

  llvm::Type *basicTy = nullptr;
  while (auto *arrayTy = llvm::dyn_cast<llvm::ArrayType>(elemTy)) {
    basicTy = elemTy = arrayTy->getElementType();
    dim++;
  }

  LLVM_DEBUG(llvm::dbgs() << "basic type: " << *basicTy << '\n'
                          << "dimensions: " << dim << '\n';);

  auto *ptrTy = llvm::PointerType::getUnqual(basicTy);
  llvm::SmallVector<llvm::Type *, 8> newTypes{ptrTy};

  for (auto i = 1u; i < dim; ++i) {
    newTypes.push_back(llvm::PointerType::getUnqual(newTypes.back()));
  }

  std::reverse(newTypes.begin(), newTypes.end());

  if (NewTypes) {
    std::copy(newTypes.begin(), newTypes.end(), std::back_inserter(*NewTypes));
  }

  return basicTy;
}

void GetDecomposedArraySizes(const llvm::PointerType *PtrTy,
                             llvm::SmallVectorImpl<uint64_t> &Sizes) {
  if (!PtrTy) {
    return;
  }

  auto *elemTy = PtrTy->getElementType();

  while (auto *arrayTy = llvm::dyn_cast<llvm::ArrayType>(elemTy)) {
    elemTy = arrayTy->getElementType();
    Sizes.push_back(arrayTy->getNumElements());
  }

  LLVM_DEBUG(llvm::dbgs() << "dimension sizes: ";
             for (auto e
                  : Sizes) { llvm::dbgs() << e << ' '; };
             llvm::dbgs() << '\n';);

  return;
}

bool FlattenMultiDimArrayIndices(llvm::GetElementPtrInst *GEP) {
  bool changed = false;

  if (!GEP) {
    return changed;
  }

  llvm::SmallVector<uint64_t, 8> sizes;
  GetDecomposedArraySizes(
      llvm::dyn_cast<llvm::PointerType>(GEP->getPointerOperandType()), sizes);

  if (sizes.size() <= 1) {
    return changed;
  }

  llvm::SmallVector<uint64_t, 8> products;
  products.resize(sizes.size());

  products[0] = 1u;
  std::reverse(sizes.begin(), sizes.end());

  for (auto i = 1u; i < products.size(); ++i) {
    products[i] = products[i - 1] * sizes[i - 1];
  }

  LLVM_DEBUG(llvm::dbgs() << "product sizes: ";
             for (auto e
                  : products) { llvm::dbgs() << e << ' '; };
             llvm::dbgs() << '\n';);

  llvm::SmallVector<llvm::Value *, 8> scaledIndices, summedIndices;
  scaledIndices.resize(sizes.size());

  llvm::SmallVector<llvm::Value *, 8> indices{std::next(GEP->idx_begin()),
                                              GEP->idx_end()};
  std::reverse(indices.begin(), indices.end());

  llvm::IRBuilder<> builder{GEP};
  changed = true;

  for (size_t i = 0; i < scaledIndices.size(); ++i) {
    scaledIndices[i] = builder.CreateMul(
        indices[i], builder.getInt64(products[i]), "scaled.idx");
  }

  summedIndices.push_back(scaledIndices[0]);
  for (size_t i = 1; i < scaledIndices.size(); ++i) {
    summedIndices.push_back(builder.CreateAdd(summedIndices[i - 1],
                                              scaledIndices[i], "summed.idx"));
  }

  llvm::SmallVector<llvm::Type *, 8> newTypes;

  GetDecomposedMultiDimArrayType(
      llvm::dyn_cast<llvm::PointerType>(GEP->getPointerOperandType()),
      &newTypes);

  auto *ptrCast = llvm::CastInst::CreatePointerCast(
      GEP->getPointerOperand(), newTypes.back(), "ptrcast", GEP);

  auto *elemTy =
      llvm::dyn_cast<llvm::PointerType>(newTypes.back())->getElementType();
  auto *newGEP = llvm::GetElementPtrInst::Create(
      elemTy, ptrCast, summedIndices.back(), "decomposed.ptr", GEP);

  if (auto *dbg = GEP->getMetadata("dbg")) {
    newGEP->setMetadata("dbg", dbg);
  }

  GEP->replaceAllUsesWith(newGEP);
  GEP->eraseFromParent();

  return changed;
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
      &newTypes);

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

