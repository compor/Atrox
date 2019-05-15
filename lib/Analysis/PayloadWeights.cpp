//
//
//

#include "Atrox/Analysis/PayloadWeights.hpp"

#include "llvm/Analysis/LoopInfo.h"
// using llvm::Loop

#include "llvm/IR/Instruction.h"
// using llvm::Instruction

#include "llvm/IR/Instructions.h"
// using llvm::LoadInst
// using llvm::StoreInst
// using llvm::AllocaInst
// using llvm::CastInst
// using llvm::TerminatorInst
// using llvm::GetElementPtrInst

#include "llvm/IR/IntrinsicInst.h"
// using llvm::DbgInfoIntrinsic
// using llvm::MemIntrinsic

#include "llvm/IR/InstVisitor.h"
// using llvm::InstVisitor

#include "llvm/ADT/SmallPtrSet.h"
// using llvm::SmallPtrSet

#include <cassert>
// using assert

namespace atrox {

namespace {

PayloadWeightTy &operator+=(PayloadWeightTy &lhs,
                            const WeightedPayloadType &rhs) {
  lhs += static_cast<PayloadWeightTy>(rhs);
  return lhs;
}

class PayloadWeightCalculator
    : public llvm::InstVisitor<PayloadWeightCalculator> {
  PayloadWeightTy m_Weight;

public:
  PayloadWeightCalculator() : m_Weight(0) {}

  PayloadWeightTy getWeight() const { return m_Weight; }
  void reset() { m_Weight = 0; }

  void visitLoadInst(llvm::LoadInst &Inst) {
    m_Weight += WeightedPayloadType::Memory;
  }

  void visitCastInst(llvm::CastInst &Inst) {
    m_Weight += WeightedPayloadType::Cast;
  }

  void visitCallInst(llvm::CallInst &Inst) {
    m_Weight += WeightedPayloadType::Call;
  }

  void visitStoreInst(llvm::StoreInst &Inst) {
    m_Weight += WeightedPayloadType::Memory;
  }

  void visitInstruction(llvm::Instruction &Inst) {
    m_Weight += WeightedPayloadType::Instruction;
  }

  void visitDbgInfoIntrinsic(llvm::DbgInfoIntrinsic &Inst) {
    m_Weight += WeightedPayloadType::DebugIntrinsic;
  }

  void visitAllocaInst(llvm::AllocaInst &Inst) {
    m_Weight += WeightedPayloadType::Memory;
  }

  void visitGetElementPtrInst(llvm::GetElementPtrInst &Inst) {
    m_Weight += WeightedPayloadType::Memory;
  }

  void visitMemIntrinsic(llvm::MemIntrinsic &Inst) {
    m_Weight += WeightedPayloadType::Memory;
  }

  void visitTerminatorInst(llvm::TerminatorInst &Inst) {
    auto *br = llvm::dyn_cast<llvm::BranchInst>(&Inst);

    if (br && br->isUnconditional())
      m_Weight += WeightedPayloadType::Minimum;
    else
      m_Weight += WeightedPayloadType::Instruction;
  }
};

} // namespace

BlockPayloadMapTy CalculatePayloadWeight(
    const llvm::SmallVectorImpl<llvm::BasicBlock *> &Blocks) {
  BlockPayloadMapTy blockPayloadMap;
  PayloadWeightCalculator pwc;

  for (auto *e : Blocks) {
    pwc.reset();
    pwc.visit(*e);
    blockPayloadMap.emplace(e, pwc.getWeight());
  }

  return blockPayloadMap;
}

} // namespace atrox

