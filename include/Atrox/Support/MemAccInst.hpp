//
//
//

#pragma once

#include "Atrox/Config.hpp"

#include "llvm/IR/Instruction.h"
// using llvm::Instruction

#include "llvm/IR/Instructions.h"
// using llvm::LoadInst
// using llvm::StoreInst
// using llvm::CallInst

#include "llvm/IR/IntrinsicInst.h"
// using llvm::MemIntrinsic

#include "llvm/IR/InstVisitor.h"
// using llvm::InstVisitor

#include "llvm/ADT/SmallVector.h"
// using llvm::SmallVector

namespace atrox {

/// Utility proxy to wrap the common members of LoadInst and StoreInst.
///
/// This works like the LLVM utility class CallSite, ie. it forwards all calls
/// to either a LoadInst, StoreInst, MemIntrinsic or MemTransferInst.
/// It is similar to LLVM's utility classes IntrinsicInst, MemIntrinsic,
/// MemTransferInst, etc. in that it offers a common interface, but does not act
/// as a fake base class.
/// It is similar to StringRef and ArrayRef in that it holds a pointer to the
/// referenced object and should be passed by-value as it is small enough.
///
/// This proxy can either represent a LoadInst instance, a StoreInst instance,
/// a MemIntrinsic instance (memset, memmove, memcpy), a CallInst instance or a
/// nullptr (only creatable using the default constructor); never an Instruction
/// that is neither of the above mentioned. When representing a nullptr, only
/// the following methods are defined:
/// isNull(), isInstruction(), isLoad(), isStore(), ..., isMemTransferInst(),
/// operator bool(), operator!()
///
/// The functions isa, cast, cast_or_null, dyn_cast are modeled to resemble
/// those from llvm/Support/Casting.h. Partial template function specialization
/// is currently not supported in C++ such that those cannot be used directly.
/// (llvm::isa could, but then llvm::cast etc. would not have the expected
/// behavior)

class MemAccInst {
private:
  llvm::Instruction *I;

public:
  MemAccInst() : I(nullptr) {}
  MemAccInst(const MemAccInst &Inst) : I(Inst.I) {}
  /* implicit */ MemAccInst(llvm::LoadInst &LI) : I(&LI) {}
  /* implicit */ MemAccInst(llvm::LoadInst *LI) : I(LI) {}
  /* implicit */ MemAccInst(llvm::StoreInst &SI) : I(&SI) {}
  /* implicit */ MemAccInst(llvm::StoreInst *SI) : I(SI) {}
  /* implicit */ MemAccInst(llvm::MemIntrinsic *MI) : I(MI) {}
  /* implicit */ MemAccInst(llvm::CallInst *CI) : I(CI) {}
  explicit MemAccInst(llvm::Instruction &I) : I(&I) { assert(isa(I)); }
  explicit MemAccInst(llvm::Instruction *I) : I(I) { assert(isa(I)); }

  static bool isa(const llvm::Value &V) {
    return llvm::isa<llvm::LoadInst>(V) || llvm::isa<llvm::StoreInst>(V) ||
           llvm::isa<llvm::CallInst>(V) || llvm::isa<llvm::MemIntrinsic>(V);
  }
  static bool isa(const llvm::Value *V) {
    return llvm::isa<llvm::LoadInst>(V) || llvm::isa<llvm::StoreInst>(V) ||
           llvm::isa<llvm::CallInst>(V) || llvm::isa<llvm::MemIntrinsic>(V);
  }
  static MemAccInst cast(llvm::Value &V) {
    return MemAccInst(llvm::cast<llvm::Instruction>(V));
  }
  static MemAccInst cast(llvm::Value *V) {
    return MemAccInst(llvm::cast<llvm::Instruction>(V));
  }
  static MemAccInst cast_or_null(llvm::Value &V) {
    return MemAccInst(llvm::cast<llvm::Instruction>(V));
  }
  static MemAccInst cast_or_null(llvm::Value *V) {
    if (!V)
      return MemAccInst();
    return MemAccInst(llvm::cast<llvm::Instruction>(V));
  }
  static MemAccInst dyn_cast(llvm::Value &V) {
    if (isa(V))
      return MemAccInst(llvm::cast<llvm::Instruction>(V));
    return MemAccInst();
  }
  static MemAccInst dyn_cast(llvm::Value *V) {
    assert(V);
    if (isa(V))
      return MemAccInst(llvm::cast<llvm::Instruction>(V));
    return MemAccInst();
  }

  MemAccInst &operator=(const MemAccInst &Inst) {
    I = Inst.I;
    return *this;
  }
  MemAccInst &operator=(llvm::LoadInst &LI) {
    I = &LI;
    return *this;
  }
  MemAccInst &operator=(llvm::LoadInst *LI) {
    I = LI;
    return *this;
  }
  MemAccInst &operator=(llvm::StoreInst &SI) {
    I = &SI;
    return *this;
  }
  MemAccInst &operator=(llvm::StoreInst *SI) {
    I = SI;
    return *this;
  }
  MemAccInst &operator=(llvm::MemIntrinsic &MI) {
    I = &MI;
    return *this;
  }
  MemAccInst &operator=(llvm::MemIntrinsic *MI) {
    I = MI;
    return *this;
  }
  MemAccInst &operator=(llvm::CallInst &CI) {
    I = &CI;
    return *this;
  }
  MemAccInst &operator=(llvm::CallInst *CI) {
    I = CI;
    return *this;
  }

  llvm::Instruction *get() const {
    assert(I && "Unexpected nullptr!");
    return I;
  }
  operator llvm::Instruction *() const { return asInstruction(); }
  llvm::Instruction *operator->() const { return get(); }

  explicit operator bool() const { return isInstruction(); }
  bool operator!() const { return isNull(); }

  llvm::Value *getValueOperand() const {
    if (isLoad())
      return asLoad();
    if (isStore())
      return asStore()->getValueOperand();
    if (isMemIntrinsic())
      return nullptr;
    if (isCallInst())
      return nullptr;
    llvm_unreachable("Operation not supported on nullptr");
  }
  llvm::Value *getPointerOperand() const {
    if (isLoad())
      return asLoad()->getPointerOperand();
    if (isStore())
      return asStore()->getPointerOperand();
    if (isMemIntrinsic())
      return asMemIntrinsic()->getRawDest();
    if (isCallInst())
      return nullptr;
    llvm_unreachable("Operation not supported on nullptr");
  }

  unsigned getAlignment() const {
    if (isLoad())
      return asLoad()->getAlignment();
    if (isStore())
      return asStore()->getAlignment();
    if (isMemTransferInst())
      return std::min(asMemTransferInst()->getDestAlignment(),
                      asMemTransferInst()->getSourceAlignment());
    if (isMemIntrinsic())
      return asMemIntrinsic()->getDestAlignment();
    if (isCallInst())
      return 0;
    llvm_unreachable("Operation not supported on nullptr");
  }
  bool isVolatile() const {
    if (isLoad())
      return asLoad()->isVolatile();
    if (isStore())
      return asStore()->isVolatile();
    if (isMemIntrinsic())
      return asMemIntrinsic()->isVolatile();
    if (isCallInst())
      return false;
    llvm_unreachable("Operation not supported on nullptr");
  }
  bool isSimple() const {
    if (isLoad())
      return asLoad()->isSimple();
    if (isStore())
      return asStore()->isSimple();
    if (isMemIntrinsic())
      return !asMemIntrinsic()->isVolatile();
    if (isCallInst())
      return true;
    llvm_unreachable("Operation not supported on nullptr");
  }
  llvm::AtomicOrdering getOrdering() const {
    if (isLoad())
      return asLoad()->getOrdering();
    if (isStore())
      return asStore()->getOrdering();
    if (isMemIntrinsic())
      return llvm::AtomicOrdering::NotAtomic;
    if (isCallInst())
      return llvm::AtomicOrdering::NotAtomic;
    llvm_unreachable("Operation not supported on nullptr");
  }
  bool isUnordered() const {
    if (isLoad())
      return asLoad()->isUnordered();
    if (isStore())
      return asStore()->isUnordered();
    // Copied from the Load/Store implementation of isUnordered:
    if (isMemIntrinsic())
      return !asMemIntrinsic()->isVolatile();
    if (isCallInst())
      return true;
    llvm_unreachable("Operation not supported on nullptr");
  }

  bool isNull() const { return !I; }
  bool isInstruction() const { return I; }

  llvm::Instruction *asInstruction() const { return I; }

private:
  bool isLoad() const { return I && llvm::isa<llvm::LoadInst>(I); }
  bool isStore() const { return I && llvm::isa<llvm::StoreInst>(I); }
  bool isCallInst() const { return I && llvm::isa<llvm::CallInst>(I); }
  bool isMemIntrinsic() const { return I && llvm::isa<llvm::MemIntrinsic>(I); }
  bool isMemSetInst() const { return I && llvm::isa<llvm::MemSetInst>(I); }
  bool isMemTransferInst() const {
    return I && llvm::isa<llvm::MemTransferInst>(I);
  }

  llvm::LoadInst *asLoad() const { return llvm::cast<llvm::LoadInst>(I); }
  llvm::StoreInst *asStore() const { return llvm::cast<llvm::StoreInst>(I); }
  llvm::CallInst *asCallInst() const { return llvm::cast<llvm::CallInst>(I); }
  llvm::MemIntrinsic *asMemIntrinsic() const {
    return llvm::cast<llvm::MemIntrinsic>(I);
  }
  llvm::MemSetInst *asMemSetInst() const {
    return llvm::cast<llvm::MemSetInst>(I);
  }
  llvm::MemTransferInst *asMemTransferInst() const {
    return llvm::cast<llvm::MemTransferInst>(I);
  }
};

//

struct MemAccInstVisitor : public llvm::InstVisitor<MemAccInstVisitor> {
  llvm::SmallVector<MemAccInst, 8> Accesses;

  explicit MemAccInstVisitor() = default;

  void visitInstruction(llvm::Instruction &I) {
    if (MemAccInst::isa(I)) {
      Accesses.push_back(MemAccInst{I});
    }
  }
};

} // namespace atrox

namespace llvm {

/// Specialize simplify_type for MemAccInst to enable dyn_cast and cast
///        from a MemAccInst object.
template <> struct simplify_type<atrox::MemAccInst> {
  typedef Instruction *SimpleType;
  static SimpleType getSimplifiedValue(atrox::MemAccInst &I) {
    return I.asInstruction();
  }
};

} // namespace llvm

