//
//
//

#pragma once

#include "Atrox/Config.hpp"

#include "llvm/IR/InstVisitor.h"
// using llvm::InstVisitor

#include "llvm/ADT/SmallVector.h"
// using llvm::SmallVector

namespace llvm {
class Value;
class IntrinsicInst;
class CallInst;
class Module;
} // namespace llvm

namespace atrox {

inline bool IsIgnoredIntrinsic(const llvm::Value *V);

//

class InstructionEraser : public llvm::InstVisitor<InstructionEraser> {
  llvm::SmallVector<llvm::Instruction *, 16> ToRemove;
  using Base = llvm::InstVisitor<InstructionEraser>;

public:
  using Base::visit;

  inline void reset() { ToRemove.clear(); }

  void visitIntrinsicInst(llvm::IntrinsicInst &I);
  bool process();
};

//

class CallDetector : public llvm::InstVisitor<CallDetector> {
  llvm::SmallVector<llvm::Instruction *, 16> Calls;
  llvm::Module *CurM = nullptr;
  using Base = llvm::InstVisitor<CallDetector>;

public:
  using Base::visit;

  explicit CallDetector(llvm::Module *M = nullptr) : CurM(M) {}

  inline void reset() { Calls.clear(); }

  void visitIntrinsicInst(llvm::IntrinsicInst &I) {}
  void visitCallInst(llvm::CallInst &I);

  explicit operator bool() const { return !Calls.empty(); }
};

} // namespace atrox
