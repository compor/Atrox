//
//
//

#pragma once

#include "Atrox/Config.hpp"

#include "Atrox/Analysis/LoopBoundsAnalyzer.hpp"

#include "llvm/Analysis/ScalarEvolution.h"
// using llvm::ScalarEvolution
// using llvm::SCEV

#include "llvm/Analysis/ScalarEvolutionExpressions.h"
// using llvm::SCEVConstant
// using llvm::SCEVAddRecExpr

#include "llvm/ADT/SmallPtrSet.h"
// using llvm::SmallPtrSetImpl
// using llvm::SmallPtrSet

#include "llvm/ADT/SmallVector.h"
// using llvm::SmallVector

#include "llvm/Support/raw_ostream.h"
// using llvm::raw_ostream

#include "llvm/Support/Debug.h"
// using LLVM_DEBUG macro
// using llvm::dbgs
// using llvm::errs

#include <map>
// using std::map

#include <cassert>
// using cassert

#define DEBUG_TYPE "atrox-lba"

namespace {

bool isOuterLoopOf(llvm::Loop *PossibleOuterLoop, llvm::Loop *QueryLoop) {
  while (QueryLoop->getParentLoop()) {
    QueryLoop = QueryLoop->getParentLoop();

    if (PossibleOuterLoop == QueryLoop) {
      return true;
    }
  }

  return false;
}

llvm::PHINode *GetInductionVariable(llvm::Loop *L, llvm::ScalarEvolution *SE) {
  llvm::PHINode *InnerIndexVar = L->getCanonicalInductionVariable();

  if (InnerIndexVar)
    return InnerIndexVar;

  if (L->getLoopLatch() == nullptr || L->getLoopPredecessor() == nullptr)
    return nullptr;

  for (llvm::BasicBlock::iterator I = L->getHeader()->begin();
       llvm::isa<llvm::PHINode>(I); ++I) {
    llvm::PHINode *PhiVar = llvm::cast<llvm::PHINode>(I);
    llvm::Type *PhiTy = PhiVar->getType();

    if (!PhiTy->isIntegerTy() && !PhiTy->isFloatingPointTy() &&
        !PhiTy->isPointerTy())
      return nullptr;

    const llvm::SCEVAddRecExpr *AddRec =
        llvm::dyn_cast<llvm::SCEVAddRecExpr>(SE->getSCEV(PhiVar));

    if (!AddRec || !AddRec->isAffine())
      continue;
    const llvm::SCEV *Step = AddRec->getStepRecurrence(*SE);

    if (!llvm::isa<llvm::SCEVConstant>(Step))
      continue;

    // FIXME handle loops with more than one induction variable

    return PhiVar;
  }

  return nullptr;
}

} // namespace

namespace atrox {

llvm::Value *GetBackedgeCondition(llvm::Loop *L) {
  llvm::Value *cond = nullptr;

  assert(L->getExitingBlock() && "Loop does not have a single exiting block!");

  if (auto *exiting = L->getExitingBlock()) {
    if (auto bi = llvm::dyn_cast<llvm::BranchInst>(exiting->getTerminator())) {
      assert(bi->isConditional() &&
             "Loop exiting block does not have conditional!");

      cond = bi->getCondition();
    }
  }

  return cond;
}

bool LoopBoundsAnalyzer::isValueUsedInLoopNestConditions(
    llvm::Value *V, llvm::Loop *L,
    llvm::SmallPtrSetImpl<llvm::Instruction *> *Conditions) {
  auto loops = L->getSubLoops();

  for (auto *e : loops) {
    for (auto *s : e->getSubLoops()) {
      loops.push_back(s);
    }
  }

  loops.push_back(L);

  for (auto *e : loops) {
    if (auto *be = llvm::dyn_cast_or_null<llvm::Instruction>(
            GetBackedgeCondition(e))) {
      LLVM_DEBUG(llvm::dbgs() << "condition: " << *be << '\n';);

      for (auto &op : be->operands()) {
        if (op.get() == V) {
          if (!Conditions) {
            return true;
          }

          Conditions->insert(be);
          break;
        }
      }
    }
  }

  return Conditions ? !Conditions->empty() : false;
}

bool LoopBoundsAnalyzer::isValueUsedOnlyInLoopNestConditions(
    llvm::Value *V, llvm::Loop *L,
    const llvm::SmallPtrSetImpl<llvm::BasicBlock *> &Interesting) {
  auto loops = L->getSubLoops();
  loops.push_back(L);

  llvm::SmallPtrSet<llvm::Instruction *, 8> cond;
  bool used = isValueUsedInLoopNestConditions(V, L, &cond);

  if (used) {
    for (auto *u : V->users()) {
      if (auto *i = llvm::dyn_cast<llvm::Instruction>(u)) {
        if (Interesting.count(i->getParent()) && !cond.count(i)) {
          return false;
        }
      }
    }
  }

  return used ? true : false;
}

bool LoopBoundsAnalyzer::analyze(llvm::Loop *CurL) {
  assert(CurL && "Loop is empty!");
  assert(LI->getLoopFor(CurL->getHeader()) == CurL &&
         "Loop does not belong to this loop info object!");

  auto *topL = CurL;
  while (topL->getParentLoop()) {
    topL = topL->getParentLoop();
  }

  if (topL == TopL) {
    return true;
  }

  reset();
  TopL = topL;

  LLVM_DEBUG(llvm::dbgs() << "analyzing loop with header: "
                          << TopL->getHeader()->getName() << '\n';);

  llvm::SmallVector<llvm::Loop *, 8> workList;
  for (auto *e : LI->getLoopsInPreorder()) {
    if (TopL->contains(e->getHeader())) {
      workList.push_back(e);
    }
  }

  for (auto *e : workList) {
    LLVM_DEBUG(llvm::dbgs() << "subloop with header: "
                            << e->getHeader()->getName() << '\n';);

    auto found = LoopBoundsMap.emplace(std::make_pair(
        e, LoopIterationSpaceInfo{nullptr, nullptr, nullptr, 0u}));

    // induction variable
    if (auto *ind = GetInductionVariable(e, SE)) {
      LLVM_DEBUG(llvm::dbgs() << "induction variable: " << *ind << '\n';);

      const llvm::SCEVAddRecExpr *indAR = nullptr;

      if (e != TopL) {
        indAR = llvm::dyn_cast<llvm::SCEVAddRecExpr>(SE->getSCEV(ind));
        // indAR =
        // llvm::dyn_cast<llvm::SCEVAddRecExpr>(SE->getSCEVAtScope(ind, e));
      } else {
        indAR = llvm::dyn_cast<llvm::SCEVAddRecExpr>(SE->getSCEV(ind));
      }

      if (!indAR) {
        LLVM_DEBUG(llvm::dbgs() << "induction variable is not an add "
                                   "recurrent expression\n";);
        continue;
      }

      auto &lisi = found.first->second;

      lisi.InductionVariable = ind;
      lisi.Start = const_cast<llvm::SCEV *>(indAR->getStart());
      LLVM_DEBUG(llvm::dbgs() << "induction start: " << *lisi.Start << '\n';);

      lisi.End = const_cast<llvm::SCEV *>(
          indAR->evaluateAtIteration(SE->getConstant(llvm::APInt{64, 5}), *SE));
      LLVM_DEBUG(llvm::dbgs() << "induction end: " << *lisi.End << '\n';);

      lisi.TripCount = SE->getSmallConstantMaxTripCount(e);
      LLVM_DEBUG(llvm::dbgs() << "trip count: " << lisi.TripCount << '\n';);
    }
  }

  return true;
}

void LoopBoundsAnalyzer::evaluate(llvm::Loop *TargetLoop,
                                  bool ShouldCalcMaxBounds) {
  if (!TopL) {
    return;
  }

  llvm::SmallVector<llvm::Loop *, 8> workList;
  for (auto *e : LI->getLoopsInReverseSiblingPreorder()) {
    if (TopL->contains(e->getHeader())) {
      workList.push_back(e);
    }
  }

  for (size_t i = 0; i < workList.size(); ++i) {
    auto &info = LoopBoundsMap[workList[i]];
    auto *outerL = workList[i];

    if (auto *startAR = llvm::dyn_cast<llvm::SCEVAddRecExpr>(info.Start)) {
      info.Start = const_cast<llvm::SCEV *>(startAR->evaluateAtIteration(
          SE->getConstant(llvm::APInt{64, 0}), *SE));
    }

    uint64_t val = (TargetLoop == outerL || ShouldCalcMaxBounds) ? 5 : 0;
    if (auto *endAR = llvm::dyn_cast<llvm::SCEVAddRecExpr>(info.End)) {
      info.End = const_cast<llvm::SCEV *>(endAR->evaluateAtIteration(
          SE->getConstant(llvm::APInt{64, val}), *SE));
    }

    for (size_t j = i + 1; j < workList.size(); ++j) {
      auto *innerL = workList[j];
      if (innerL->getLoopDepth() <= outerL->getLoopDepth()) {
        break;
      }

      auto &info = LoopBoundsMap[innerL];

      if (auto *startAR = llvm::dyn_cast<llvm::SCEVAddRecExpr>(
              SE->getSCEVAtScope(info.Start, innerL))) {
        info.Start = const_cast<llvm::SCEV *>(startAR->evaluateAtIteration(
            SE->getConstant(llvm::APInt{64, 0}), *SE));
      }

      if (TargetLoop != outerL || ShouldCalcMaxBounds) {
        if (auto *endAR = llvm::dyn_cast<llvm::SCEVAddRecExpr>(
                SE->getSCEVAtScope(info.End, innerL))) {
          info.End = const_cast<llvm::SCEV *>(endAR->evaluateAtIteration(
              SE->getConstant(llvm::APInt{64, val}), *SE));
        }
      }
    }

    if (TargetLoop == outerL) {
      break;
    }
  }
}

bool LoopBoundsAnalyzer::isValueOuterLoopInductionVariable(llvm::Value *V,
                                                           llvm::Loop *L) {
  for (auto &e : LoopBoundsMap) {
    if (e.second.InductionVariable == V) {
      return isOuterLoopOf(e.first, L);
    }
  }

  return false;
}

bool LoopBoundsAnalyzer::isValueInnerLoopInductionVariable(llvm::Value *V,
                                                           llvm::Loop *L) {
  for (auto &e : LoopBoundsMap) {
    if (e.second.InductionVariable == V) {
      return isOuterLoopOf(L, e.first);
    }
  }

  return false;
}

llvm::Optional<LoopIterationSpaceInfo>
LoopBoundsAnalyzer::getInfo(llvm::Loop *L) const {
  auto found = LoopBoundsMap.find(L);
  if (found != LoopBoundsMap.end()) {
    return found->second;
  }

  return llvm::None;
}

llvm::Optional<LoopIterationSpaceInfo>
LoopBoundsAnalyzer::getInfo(llvm::Value *IndVar) const {
  for (auto &e : LoopBoundsMap) {
    if (e.second.InductionVariable == IndVar) {
      return e.second;
    }
  }

  return llvm::None;
}

void LoopBoundsAnalyzer::print(llvm::raw_ostream &OS) const {
  for (auto &e : LoopBoundsMap) {
    OS << "loop with header: " << e.first->getHeader()->getName() << '\n';
    OS << "start: ";
    e.second.Start->print(OS);
    OS << '\n';
    OS << "end: ";
    e.second.End->print(OS);
    OS << '\n';
  }
}

} // namespace atrox

