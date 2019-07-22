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
} // namespace llvm

namespace atrox {

inline bool IsIgnoredIntrinsic(const llvm::Value *V);

//

class InstructionEraser : public llvm::InstVisitor<InstructionEraser> {
  llvm::SmallVector<llvm::Instruction *, 16> ToRemove;

public:
  inline void reset() { ToRemove.clear(); }

  void visitIntrinsicInst(llvm::IntrinsicInst &I);
  bool process();
};

} // namespace atrox
