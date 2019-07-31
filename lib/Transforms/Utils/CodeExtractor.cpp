//===- CodeExtractor.cpp - Pull code region into a new function -----------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements the interface to tear out a code region, such as an
// individual loop or a parallel section, into a new function, replacing it with
// a call to the new function.
//
//===----------------------------------------------------------------------===//

#include "Atrox/Transforms/Utils/CodeExtractor.hpp"

#include "Atrox/Support/IR/ArgUtils.hpp"
#include "Atrox/Support/IR/GeneralUtils.hpp"
#include "Atrox/Transforms/DecomposeMultiDimArrayRefs.hpp"

#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/Optional.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Analysis/BlockFrequencyInfo.h"
#include "llvm/Analysis/BlockFrequencyInfoImpl.h"
#include "llvm/Analysis/BranchProbabilityInfo.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/ScalarEvolutionExpressions.h"
#include "llvm/IR/Argument.h"
#include "llvm/IR/Attributes.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Constant.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/GlobalValue.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/MDBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/User.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Pass.h"
#include "llvm/Support/BlockFrequency.h"
#include "llvm/Support/BranchProbability.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include <cassert>
#include <cstdint>
#include <iterator>
#include <map>
#include <set>
#include <utility>
#include <vector>

using namespace llvm;
using namespace atrox;
using ProfileCount = Function::ProfileCount;

#define DEBUG_TYPE "atrox-code-extractor"

// Provide a command-line option to aggregate function arguments into a struct
// for functions produced by the code extractor. This is useful when converting
// extracted functions to pthread-based code, as only one argument (void*) can
// be passed in to pthread_create().
static cl::opt<bool> AggregateArgsOpt(
    "atrox-aggregate-extracted-args", cl::Hidden,
    cl::desc("Aggregate arguments to code-extracted functions"));

static cl::opt<bool>
    FlattenArrayAccesses("atrox-flatten-array-accesses", cl::Hidden,
                         cl::init(false),
                         cl::desc("Flatten multi-dimensional array accesses"));

#if !defined(NDEBUG)
static cl::opt<bool>
    VerifyOption("atrox-verify", cl::Hidden, cl::init(true),
                 cl::desc("Perform verification of code-extracted functions"));
#endif // !defined(NDEBUG)

/// Test whether a block is valid for extraction.
static bool isBlockValidForExtraction(const BasicBlock &BB,
                                      const SetVector<BasicBlock *> &Result,
                                      bool AllowVarArgs, bool AllowAlloca) {
  // taking the address of a basic block moved to another function is illegal
  if (BB.hasAddressTaken())
    return false;

  // don't hoist code that uses another basicblock address, as it's likely to
  // lead to unexpected behavior, like cross-function jumps
  SmallPtrSet<User const *, 16> Visited;
  SmallVector<User const *, 16> ToVisit;

  for (Instruction const &Inst : BB)
    ToVisit.push_back(&Inst);

  while (!ToVisit.empty()) {
    User const *Curr = ToVisit.pop_back_val();
    if (!Visited.insert(Curr).second)
      continue;
    if (isa<BlockAddress const>(Curr))
      return false; // even a reference to self is likely to be not compatible

    if (isa<Instruction>(Curr) && cast<Instruction>(Curr)->getParent() != &BB)
      continue;

    for (auto const &U : Curr->operands()) {
      if (auto *UU = dyn_cast<User>(U))
        ToVisit.push_back(UU);
    }
  }

  // If explicitly requested, allow vastart and alloca. For invoke instructions
  // verify that extraction is valid.
  for (BasicBlock::const_iterator I = BB.begin(), E = BB.end(); I != E; ++I) {
    if (isa<AllocaInst>(I)) {
      if (!AllowAlloca)
        return false;
      continue;
    }

    if (const auto *II = dyn_cast<InvokeInst>(I)) {
      // Unwind destination (either a landingpad, catchswitch, or cleanuppad)
      // must be a part of the subgraph which is being extracted.
      if (auto *UBB = II->getUnwindDest())
        if (!Result.count(UBB))
          return false;
      continue;
    }

    // All catch handlers of a catchswitch instruction as well as the unwind
    // destination must be in the subgraph.
    if (const auto *CSI = dyn_cast<CatchSwitchInst>(I)) {
      if (auto *UBB = CSI->getUnwindDest())
        if (!Result.count(UBB))
          return false;
      for (auto *HBB : CSI->handlers())
        if (!Result.count(const_cast<BasicBlock *>(HBB)))
          return false;
      continue;
    }

    // Make sure that entire catch handler is within subgraph. It is sufficient
    // to check that catch return's block is in the list.
    if (const auto *CPI = dyn_cast<CatchPadInst>(I)) {
      for (const auto *U : CPI->users())
        if (const auto *CRI = dyn_cast<CatchReturnInst>(U))
          if (!Result.count(const_cast<BasicBlock *>(CRI->getParent())))
            return false;
      continue;
    }

    // And do similar checks for cleanup handler - the entire handler must be
    // in subgraph which is going to be extracted. For cleanup return should
    // additionally check that the unwind destination is also in the subgraph.
    if (const auto *CPI = dyn_cast<CleanupPadInst>(I)) {
      for (const auto *U : CPI->users())
        if (const auto *CRI = dyn_cast<CleanupReturnInst>(U))
          if (!Result.count(const_cast<BasicBlock *>(CRI->getParent())))
            return false;
      continue;
    }
    if (const auto *CRI = dyn_cast<CleanupReturnInst>(I)) {
      if (auto *UBB = CRI->getUnwindDest())
        if (!Result.count(UBB))
          return false;
      continue;
    }

    if (const CallInst *CI = dyn_cast<CallInst>(I))
      if (const Function *F = CI->getCalledFunction())
        if (F->getIntrinsicID() == Intrinsic::vastart) {
          if (AllowVarArgs)
            continue;
          else
            return false;
        }
  }

  return true;
}

/// Build a set of blocks to extract if the input blocks are viable.
static SetVector<BasicBlock *>
buildExtractionBlockSet(ArrayRef<BasicBlock *> BBs, DominatorTree *DT,
                        bool AllowVarArgs, bool AllowAlloca) {
  assert(!BBs.empty() && "The set of blocks to extract must be non-empty");
  SetVector<BasicBlock *> Result;

  // Loop over the blocks, adding them to our set-vector, and aborting with an
  // empty set if we encounter invalid blocks.
  for (BasicBlock *BB : BBs) {
    // If this block is dead, don't process it.
    if (DT && !DT->isReachableFromEntry(BB))
      continue;

    if (!Result.insert(BB))
      llvm_unreachable("Repeated basic blocks in extraction input");
  }

  for (auto *BB : Result) {
    if (!isBlockValidForExtraction(*BB, Result, AllowVarArgs, AllowAlloca))
      return {};

    // Make sure that the first block is not a landing pad.
    if (BB == Result.front()) {
      if (BB->isEHPad()) {
        LLVM_DEBUG(dbgs() << "The first block cannot be an unwind block\n");
        return {};
      }
      continue;
    }

    // All blocks other than the first must not have predecessors outside of
    // the subgraph which is being extracted.
    for (auto *PBB : predecessors(BB)) {
      if (!Result.count(PBB)) {
        LLVM_DEBUG(dbgs() << "Block with term: " << *BB->getTerminator()
                          << " has predecessor from outside the region: "
                          << *PBB->getTerminator() << '\n';);

        // return {};
      }
    }
  }

  return Result;
}

CodeExtractor::CodeExtractor(ArrayRef<BasicBlock *> BBs, Loop &L,
                             iteratorrecognition::IteratorInfo *IterInfo,
                             LoopBoundsAnalyzer *LBA, DominatorTree *DT,
                             bool AggregateArgs, BlockFrequencyInfo *BFI,
                             BranchProbabilityInfo *BPI, bool AllowVarArgs,
                             bool AllowAlloca)
    : CurL(L), IterInfo(IterInfo), LBA(LBA), DT(DT),
      AggregateArgs(AggregateArgs || AggregateArgsOpt), BFI(BFI), BPI(BPI),
      AllowVarArgs(AllowVarArgs),
      Blocks(buildExtractionBlockSet(BBs, DT, AllowVarArgs, AllowAlloca)),
      Accesses(nullptr) {}

/// definedInRegion - Return true if the specified value is defined in the
/// extracted region.
static bool definedInRegion(const SetVector<BasicBlock *> &Blocks, Value *V) {
  if (Instruction *I = dyn_cast<Instruction>(V))
    if (Blocks.count(I->getParent()))
      return true;
  return false;
}

/// definedInCaller - Return true if the specified value is defined in the
/// function being code extracted, but not in the region being extracted.
/// These values must be passed in as live-ins to the function.
static bool definedInCaller(const SetVector<BasicBlock *> &Blocks, Value *V) {
  if (isa<Argument>(V))
    return true;
  if (Instruction *I = dyn_cast<Instruction>(V))
    if (!Blocks.count(I->getParent()))
      return true;
  return false;
}

static BasicBlock *getCommonExitBlock(const SetVector<BasicBlock *> &Blocks) {
  BasicBlock *CommonExitBlock = nullptr;
  auto hasNonCommonExitSucc = [&](BasicBlock *Block) {
    for (auto *Succ : successors(Block)) {
      // Internal edges, ok.
      if (Blocks.count(Succ))
        continue;
      if (!CommonExitBlock) {
        CommonExitBlock = Succ;
        continue;
      }
      if (CommonExitBlock == Succ)
        continue;

      return true;
    }
    return false;
  };

  if (any_of(Blocks, hasNonCommonExitSucc))
    return nullptr;

  return CommonExitBlock;
}

bool CodeExtractor::isLegalToShrinkwrapLifetimeMarkers(
    Instruction *Addr) const {
  AllocaInst *AI = cast<AllocaInst>(Addr->stripInBoundsConstantOffsets());
  Function *Func = (*Blocks.begin())->getParent();
  for (BasicBlock &BB : *Func) {
    if (Blocks.count(&BB))
      continue;
    for (Instruction &II : BB) {
      if (isa<DbgInfoIntrinsic>(II))
        continue;

      unsigned Opcode = II.getOpcode();
      Value *MemAddr = nullptr;
      switch (Opcode) {
      case Instruction::Store:
      case Instruction::Load: {
        if (Opcode == Instruction::Store) {
          StoreInst *SI = cast<StoreInst>(&II);
          MemAddr = SI->getPointerOperand();
        } else {
          LoadInst *LI = cast<LoadInst>(&II);
          MemAddr = LI->getPointerOperand();
        }
        // Global variable can not be aliased with locals.
        if (dyn_cast<Constant>(MemAddr))
          break;
        Value *Base = MemAddr->stripInBoundsConstantOffsets();
        if (!dyn_cast<AllocaInst>(Base) || Base == AI)
          return false;
        break;
      }
      default: {
        IntrinsicInst *IntrInst = dyn_cast<IntrinsicInst>(&II);
        if (IntrInst) {
          if (IntrInst->getIntrinsicID() == Intrinsic::lifetime_start ||
              IntrInst->getIntrinsicID() == Intrinsic::lifetime_end)
            break;
          return false;
        }
        // Treat all the other cases conservatively if it has side effects.
        if (II.mayHaveSideEffects())
          return false;
      }
      }
    }
  }

  return true;
}

BasicBlock *
CodeExtractor::findOrCreateBlockForHoisting(BasicBlock *CommonExitBlock) {
  BasicBlock *SinglePredFromOutlineRegion = nullptr;
  assert(!Blocks.count(CommonExitBlock) &&
         "Expect a block outside the region!");
  for (auto *Pred : predecessors(CommonExitBlock)) {
    if (!Blocks.count(Pred))
      continue;
    if (!SinglePredFromOutlineRegion) {
      SinglePredFromOutlineRegion = Pred;
    } else if (SinglePredFromOutlineRegion != Pred) {
      SinglePredFromOutlineRegion = nullptr;
      break;
    }
  }

  if (SinglePredFromOutlineRegion)
    return SinglePredFromOutlineRegion;

#ifndef NDEBUG
  auto getFirstPHI = [](BasicBlock *BB) {
    BasicBlock::iterator I = BB->begin();
    PHINode *FirstPhi = nullptr;
    while (I != BB->end()) {
      PHINode *Phi = dyn_cast<PHINode>(I);
      if (!Phi)
        break;
      if (!FirstPhi) {
        FirstPhi = Phi;
        break;
      }
    }
    return FirstPhi;
  };
  // If there are any phi nodes, the single pred either exists or has already
  // be created before code extraction.
  assert(!getFirstPHI(CommonExitBlock) && "Phi not expected");
#endif

  BasicBlock *NewExitBlock = CommonExitBlock->splitBasicBlock(
      CommonExitBlock->getFirstNonPHI()->getIterator());

  for (auto PI = pred_begin(CommonExitBlock), PE = pred_end(CommonExitBlock);
       PI != PE;) {
    BasicBlock *Pred = *PI++;
    if (Blocks.count(Pred))
      continue;
    Pred->getTerminator()->replaceUsesOfWith(CommonExitBlock, NewExitBlock);
  }
  // Now add the old exit block to the outline region.
  Blocks.insert(CommonExitBlock);
  return CommonExitBlock;
}

void CodeExtractor::prepare() {
  Inputs.clear();
  Outputs.clear();
  InputToOutputMap.clear();
  OutputToInputMap.clear();

  findInputsOutputs(Inputs, Outputs);
  findGlobalInputsOutputs(Inputs, Outputs);

  mapInputsOutputs(Inputs, Outputs, InputToOutputMap, OutputToInputMap);

  if (IterInfo) {
    ReorderInputs(Inputs, *IterInfo);
  }

  findStackAllocatable();
}

void CodeExtractor::findStackAllocatable() {
  if (!LBA) {
    LLVM_DEBUG(
        llvm::dbgs()
        << "skipping because there is no loop bounds analyzer information\n");
    return;
  }

  llvm::SmallPtrSet<llvm::BasicBlock *, 8> scopeBlocks{Blocks.begin(),
                                                       Blocks.end()};

  auto evalLBA = *LBA;
  evalLBA.evaluate(0, 5, &CurL);

  for (auto *v : Inputs) {
    auto isCondUse =
        LBA->isValueUsedOnlyInLoopNestConditions(v, &CurL, scopeBlocks);
    auto isOuterIndVar = LBA->isValueOuterLoopInductionVariable(v, &CurL);
    auto isInnerIndVar = LBA->isValueInnerLoopInductionVariable(v, &CurL);

    auto isBoth = isCondUse && isOuterIndVar;
    if (isBoth) {
      LLVM_DEBUG(llvm::dbgs() << "Input is both used in condition and as "
                                 "outer induction variable: "
                              << *v << '\n';);
    }
    assert(!isBoth && "Input is of both kinds!");

    isBoth = isCondUse && isInnerIndVar;
    if (isBoth) {
      LLVM_DEBUG(llvm::dbgs() << "Input is both used in condition and as "
                                 "inner induction variable: "
                              << *v << '\n';);
    }
    assert(!isBoth && "Input is of both kinds!");

    if (isInnerIndVar) {
      auto lbInfoOrErr = evalLBA.getInfo(v);
      if (!lbInfoOrErr) {
        LLVM_DEBUG(llvm::dbgs() << "Missing loop iteration space info!\n";);
        // assert?
      }
      auto lbInfo = *lbInfoOrErr;

      assert(llvm::dyn_cast_or_null<llvm::SCEVConstant>(lbInfo.Start) &&
             "Inner induction variable is not constant!");

      StackAllocaInits.push_back(nullptr);
      StackAllocas.insert(v);
    }

    if (isOuterIndVar) {
      auto lbInfoOrErr = evalLBA.getInfo(v);
      if (!lbInfoOrErr) {
        LLVM_DEBUG(llvm::dbgs() << "Missing loop iteration space info!\n";);
        // assert?
      }
      auto lbInfo = *lbInfoOrErr;

      auto *start = llvm::dyn_cast_or_null<llvm::SCEVConstant>(lbInfo.Start);
      llvm::ConstantInt *initVal = nullptr;
      if (start) {
        // FIXME maybe check that the induction var is an integer type before
        // casting
        initVal = llvm::dyn_cast<llvm::ConstantInt>(
            llvm::ConstantInt::get(lbInfo.InductionVariable->getType(),
                                   start->getValue()->getZExtValue()));
      } else {
        // TODO what happens here if the value is used in the loop in a
        // division or a subtraction that results in a negative number?
        uint64_t val = 354;
        initVal = llvm::dyn_cast<llvm::ConstantInt>(
            llvm::ConstantInt::get(lbInfo.InductionVariable->getType(), val));

        LLVM_DEBUG(llvm::dbgs()
                       << "outer induction variable: "
                       << *lbInfo.InductionVariable
                       << " does not have a constant start value: "
                       << *lbInfo.Start << " set to: " << val << '\n';);
      }

      StackAllocaInits.push_back(initVal);
      StackAllocas.insert(v);
    }

    if (isCondUse) {
      auto *initVal = llvm::ConstantInt::get(v->getType(), 5);
      StackAllocaInits.push_back(initVal);
      StackAllocas.insert(v);
    }
  }
}

void CodeExtractor::findAllocas(ValueSet &SinkCands, ValueSet &HoistCands,
                                BasicBlock *&ExitBlock) const {
  Function *Func = (*Blocks.begin())->getParent();
  ExitBlock = getCommonExitBlock(Blocks);

  for (BasicBlock &BB : *Func) {
    if (Blocks.count(&BB))
      continue;
    for (Instruction &II : BB) {
      auto *AI = dyn_cast<AllocaInst>(&II);
      if (!AI)
        continue;

      // Find the pair of life time markers for address 'Addr' that are either
      // defined inside the outline region or can legally be shrinkwrapped into
      // the outline region. If there are not other untracked uses of the
      // address, return the pair of markers if found; otherwise return a pair
      // of nullptr.
      auto GetLifeTimeMarkers =
          [&](Instruction *Addr, bool &SinkLifeStart,
              bool &HoistLifeEnd) -> std::pair<Instruction *, Instruction *> {
        Instruction *LifeStart = nullptr, *LifeEnd = nullptr;

        for (User *U : Addr->users()) {
          IntrinsicInst *IntrInst = dyn_cast<IntrinsicInst>(U);
          if (IntrInst) {
            if (IntrInst->getIntrinsicID() == Intrinsic::lifetime_start) {
              // Do not handle the case where AI has multiple start markers.
              if (LifeStart)
                return std::make_pair<Instruction *>(nullptr, nullptr);
              LifeStart = IntrInst;
            }
            if (IntrInst->getIntrinsicID() == Intrinsic::lifetime_end) {
              if (LifeEnd)
                return std::make_pair<Instruction *>(nullptr, nullptr);
              LifeEnd = IntrInst;
            }
            continue;
          }
          // Find untracked uses of the address, bail.
          if (!definedInRegion(Blocks, U))
            return std::make_pair<Instruction *>(nullptr, nullptr);
        }

        if (!LifeStart || !LifeEnd)
          return std::make_pair<Instruction *>(nullptr, nullptr);

        SinkLifeStart = !definedInRegion(Blocks, LifeStart);
        HoistLifeEnd = !definedInRegion(Blocks, LifeEnd);
        // Do legality Check.
        if ((SinkLifeStart || HoistLifeEnd) &&
            !isLegalToShrinkwrapLifetimeMarkers(Addr))
          return std::make_pair<Instruction *>(nullptr, nullptr);

        // Check to see if we have a place to do hoisting, if not, bail.
        if (HoistLifeEnd && !ExitBlock)
          return std::make_pair<Instruction *>(nullptr, nullptr);

        return std::make_pair(LifeStart, LifeEnd);
      };

      bool SinkLifeStart = false, HoistLifeEnd = false;
      auto Markers = GetLifeTimeMarkers(AI, SinkLifeStart, HoistLifeEnd);

      if (Markers.first) {
        if (SinkLifeStart)
          SinkCands.insert(Markers.first);
        SinkCands.insert(AI);
        if (HoistLifeEnd)
          HoistCands.insert(Markers.second);
        continue;
      }

      // Follow the bitcast.
      Instruction *MarkerAddr = nullptr;
      for (User *U : AI->users()) {
        if (U->stripInBoundsConstantOffsets() == AI) {
          SinkLifeStart = false;
          HoistLifeEnd = false;
          Instruction *Bitcast = cast<Instruction>(U);
          Markers = GetLifeTimeMarkers(Bitcast, SinkLifeStart, HoistLifeEnd);
          if (Markers.first) {
            MarkerAddr = Bitcast;
            continue;
          }
        }

        // Found unknown use of AI.
        if (!definedInRegion(Blocks, U)) {
          MarkerAddr = nullptr;
          break;
        }
      }

      if (MarkerAddr) {
        if (SinkLifeStart)
          SinkCands.insert(Markers.first);
        if (!definedInRegion(Blocks, MarkerAddr))
          SinkCands.insert(MarkerAddr);
        SinkCands.insert(AI);
        if (HoistLifeEnd)
          HoistCands.insert(Markers.second);
      }
    }
  }
}

void CodeExtractor::findInputsOutputs(ValueSet &Inputs, ValueSet &Outputs) {
  for (BasicBlock *BB : Blocks) {
    // If a used value is defined outside the region, it's an input.  If an
    // instruction is used outside the region, it's an output.
    for (Instruction &II : *BB) {
      for (User::op_iterator OI = II.op_begin(), OE = II.op_end(); OI != OE;
           ++OI) {
        Value *V = *OI;
        if (definedInCaller(Blocks, V))
          Inputs.insert(V);
      }

      for (User *U : II.users())
        if (!definedInRegion(Blocks, U)) {
          Outputs.insert(&II);
          break;
        }
    }
  }
}

void CodeExtractor::findGlobalInputsOutputs(ValueSet &Inputs,
                                            ValueSet &Outputs) {
  llvm::SmallVector<GetElementPtrInst *, 16> geps;

  for (BasicBlock *BB : Blocks) {
    for (Instruction &II : *BB) {
      if (auto *gep = llvm::dyn_cast<llvm::GetElementPtrInst>(&II)) {
        geps.push_back(const_cast<llvm::GetElementPtrInst *>(gep));
      }
    }
  }

  auto toErase = std::remove_if(geps.begin(), geps.end(), [&](const auto &e) {
    return !llvm::isa<llvm::GlobalVariable>(e->getPointerOperand());
  });

  if (toErase != geps.end()) {
    geps.erase(toErase);
  }

  for (auto *gep : geps) {
    for (auto *u : gep->users()) {
      if (llvm::isa<llvm::StoreInst>(u)) {
        Outputs.insert(gep->getPointerOperand());
        break;
      }
    }

    for (auto *u : gep->users()) {
      if (!Outputs.count(gep) && llvm::isa<llvm::LoadInst>(u)) {
        Inputs.insert(gep->getPointerOperand());
        break;
      }
    }
  }
}

void CodeExtractor::mapInputsOutputs(const ValueSet &Inputs,
                                     const ValueSet &Outputs,
                                     InputToOutputMapTy &IOMap,
                                     OutputToInputMapTy &OIMap) {
  for (auto *v : Inputs) {
    auto *phi = dyn_cast<PHINode>(v);
    if (!phi) {
      continue;
    }

    for (size_t i = 0; i < phi->getNumIncomingValues(); ++i) {
      auto *incV = phi->getIncomingValue(i);
      auto found = std::find(Outputs.begin(), Outputs.end(), incV);

      if (found != Outputs.end()) {
        IOMap[v] = std::distance(Outputs.begin(), found);
        OIMap[incV] = v;
      }
    }
  }

  for (size_t i = 0; i < Outputs.size(); ++i) {
    if (Inputs.count(Outputs[i])) {
      auto found = std::find(Inputs.begin(), Inputs.end(), Outputs[i]);
      IOMap[*found] = i;
      OIMap[Outputs[i]] = *found;
    }
  }
}

/// severSplitPHINodes - If a PHI node has multiple inputs from outside of the
/// region, we need to split the entry block of the region so that the PHI node
/// is easier to deal with.
void CodeExtractor::severSplitPHINodes(BasicBlock *&Header) {
  unsigned NumPredsFromRegion = 0;
  unsigned NumPredsOutsideRegion = 0;

  if (Header != &Header->getParent()->getEntryBlock()) {
    PHINode *PN = dyn_cast<PHINode>(Header->begin());
    if (!PN)
      return; // No PHI nodes.

    // If the header node contains any PHI nodes, check to see if there is more
    // than one entry from outside the region.  If so, we need to sever the
    // header block into two.
    for (unsigned i = 0, e = PN->getNumIncomingValues(); i != e; ++i)
      if (Blocks.count(PN->getIncomingBlock(i)))
        ++NumPredsFromRegion;
      else
        ++NumPredsOutsideRegion;

    // If there is one (or fewer) predecessor from outside the region, we don't
    // need to do anything special.
    if (NumPredsOutsideRegion <= 1)
      return;
  }

  // Otherwise, we need to split the header block into two pieces: one
  // containing PHI nodes merging values from outside of the region, and a
  // second that contains all of the code for the block and merges back any
  // incoming values from inside of the region.
  BasicBlock *NewBB = SplitBlock(Header, Header->getFirstNonPHI(), DT);

  // We only want to code extract the second block now, and it becomes the new
  // header of the region.
  BasicBlock *OldPred = Header;
  Blocks.remove(OldPred);
  Blocks.insert(NewBB);
  Header = NewBB;

  // Okay, now we need to adjust the PHI nodes and any branches from within the
  // region to go to the new header block instead of the old header block.
  if (NumPredsFromRegion) {
    PHINode *PN = cast<PHINode>(OldPred->begin());
    // Loop over all of the predecessors of OldPred that are in the region,
    // changing them to branch to NewBB instead.
    for (unsigned i = 0, e = PN->getNumIncomingValues(); i != e; ++i)
      if (Blocks.count(PN->getIncomingBlock(i))) {
        TerminatorInst *TI = PN->getIncomingBlock(i)->getTerminator();
        TI->replaceUsesOfWith(OldPred, NewBB);
      }

    // Okay, everything within the region is now branching to the right block,
    // we just have to update the PHI nodes now, inserting PHI nodes into NewBB.
    BasicBlock::iterator AfterPHIs;
    for (AfterPHIs = OldPred->begin(); isa<PHINode>(AfterPHIs); ++AfterPHIs) {
      PHINode *PN = cast<PHINode>(AfterPHIs);
      // Create a new PHI node in the new region, which has an incoming value
      // from OldPred of PN.
      PHINode *NewPN = PHINode::Create(PN->getType(), 1 + NumPredsFromRegion,
                                       PN->getName() + ".ce", &NewBB->front());
      PN->replaceAllUsesWith(NewPN);
      NewPN->addIncoming(PN, OldPred);

      // Loop over all of the incoming value in PN, moving them to NewPN if they
      // are from the extracted region.
      for (unsigned i = 0; i != PN->getNumIncomingValues(); ++i) {
        if (Blocks.count(PN->getIncomingBlock(i))) {
          NewPN->addIncoming(PN->getIncomingValue(i), PN->getIncomingBlock(i));
          PN->removeIncomingValue(i);
          --i;
        }
      }
    }
  }
}

void CodeExtractor::splitReturnBlocks() {
  for (BasicBlock *Block : Blocks)
    if (ReturnInst *RI = dyn_cast<ReturnInst>(Block->getTerminator())) {
      BasicBlock *New =
          Block->splitBasicBlock(RI->getIterator(), Block->getName() + ".ret");
      if (DT) {
        // Old dominates New. New node dominates all other nodes dominated
        // by Old.
        DomTreeNode *OldNode = DT->getNode(Block);
        SmallVector<DomTreeNode *, 8> Children(OldNode->begin(),
                                               OldNode->end());

        DomTreeNode *NewNode = DT->addNewBlock(New, Block);

        for (DomTreeNode *I : Children)
          DT->changeImmediateDominator(I, NewNode);
      }
    }
}

void CodeExtractor::remapCloneBlocks() {
  remapInstructionsInBlocks(CloneBlocks, VMap);
}

Function *CodeExtractor::cloneFunction(const ValueSet &inputs,
                                       const ValueSet &outputs,
                                       BasicBlock *header,
                                       BasicBlock *newRootNode,
                                       BasicBlock *newExitNode,
                                       Function *oldFunction, Module *M) {
  LLVM_DEBUG(dbgs() << "inputs: " << inputs.size() << "\n");
  LLVM_DEBUG(dbgs() << "outputs: " << outputs.size() << "\n");

  RetTy = Type::getVoidTy(header->getContext());

  std::vector<Type *> paramTy;

  ValueSet usedInputs;
  for (Value *value : inputs) {
    if (!isInputOnly(value)) {
      LLVM_DEBUG(dbgs() << "skipping value used in func: " << *value << "\n");
      continue;
    }

    usedInputs.insert(value);
  }

  // Add the types of the input values to the function's argument list
  for (Value *value : usedInputs) {
    LLVM_DEBUG(dbgs() << "value used in func: " << *value << "\n");
    paramTy.push_back(value->getType());
  }

  // Add the types of the output values to the function's argument list.
  for (Value *output : outputs) {
    LLVM_DEBUG(dbgs() << "value used in func: " << *output << "\n");
    if (AggregateArgs)
      paramTy.push_back(output->getType());
    else {
      if (output->getType()->isPointerTy()) {
        paramTy.push_back(output->getType());
      } else {
        paramTy.push_back(PointerType::getUnqual(output->getType()));
      }
    }
  }

  LLVM_DEBUG({
    dbgs() << "Function type: " << *RetTy << " f(";
    for (Type *i : paramTy)
      dbgs() << *i << ", ";
    dbgs() << ")\n";
  });

  StructType *StructTy;
  if (AggregateArgs && (usedInputs.size() + outputs.size() > 0)) {
    StructTy = StructType::get(M->getContext(), paramTy);
    paramTy.clear();
    paramTy.push_back(PointerType::getUnqual(StructTy));
  }
  FunctionType *funcType = FunctionType::get(
      RetTy, paramTy, AllowVarArgs && oldFunction->isVarArg());

  // Create the new function
  Function *newFunction =
      Function::Create(funcType, GlobalValue::ExternalLinkage,
                       oldFunction->getName() + "_" + header->getName(), M);
  // If the old function is no-throw, so is the new one.
  if (oldFunction->doesNotThrow())
    newFunction->setDoesNotThrow();

  // Inherit the uwtable attribute if we need to.
  if (oldFunction->hasUWTable())
    newFunction->setHasUWTable();

  // Inherit all of the target dependent attributes and white-listed
  // target independent attributes.
  //  (e.g. If the extracted region contains a call to an x86.sse
  //  instruction we need to make sure that the extracted region has the
  //  "target-features" attribute allowing it to be lowered.
  // FIXME: This should be changed to check to see if a specific
  //           attribute can not be inherited.
  for (const auto &Attr : oldFunction->getAttributes().getFnAttributes()) {
    if (Attr.isStringAttribute()) {
      if (Attr.getKindAsString() == "thunk")
        continue;
    } else
      switch (Attr.getKindAsEnum()) {
      // Those attributes cannot be propagated safely. Explicitly list them
      // here so we get a warning if new attributes are added. This list also
      // includes non-function attributes.
      case Attribute::Alignment:
      case Attribute::AllocSize:
      case Attribute::ArgMemOnly:
      case Attribute::Builtin:
      case Attribute::ByVal:
      case Attribute::Convergent:
      case Attribute::Dereferenceable:
      case Attribute::DereferenceableOrNull:
      case Attribute::InAlloca:
      case Attribute::InReg:
      case Attribute::InaccessibleMemOnly:
      case Attribute::InaccessibleMemOrArgMemOnly:
      case Attribute::JumpTable:
      case Attribute::Naked:
      case Attribute::Nest:
      case Attribute::NoAlias:
      case Attribute::NoBuiltin:
      case Attribute::NoCapture:
      case Attribute::NoReturn:
      case Attribute::None:
      case Attribute::NonNull:
      case Attribute::ReadNone:
      case Attribute::ReadOnly:
      case Attribute::Returned:
      case Attribute::ReturnsTwice:
      case Attribute::SExt:
      case Attribute::Speculatable:
      case Attribute::StackAlignment:
      case Attribute::StructRet:
      case Attribute::SwiftError:
      case Attribute::SwiftSelf:
      case Attribute::WriteOnly:
      case Attribute::ZExt:
      case Attribute::EndAttrKinds:
        continue;
      // Those attributes should be safe to propagate to the extracted function.
      case Attribute::AlwaysInline:
      case Attribute::Cold:
      case Attribute::NoRecurse:
      case Attribute::InlineHint:
      case Attribute::MinSize:
      case Attribute::NoDuplicate:
      case Attribute::NoImplicitFloat:
      case Attribute::NoInline:
      case Attribute::NonLazyBind:
      case Attribute::NoRedZone:
      case Attribute::NoUnwind:
      case Attribute::OptForFuzzing:
      case Attribute::OptimizeNone:
      case Attribute::OptimizeForSize:
      case Attribute::SafeStack:
      case Attribute::ShadowCallStack:
      case Attribute::SanitizeAddress:
      case Attribute::SanitizeMemory:
      case Attribute::SanitizeThread:
      case Attribute::SanitizeHWAddress:
      case Attribute::StackProtect:
      case Attribute::StackProtectReq:
      case Attribute::StackProtectStrong:
      case Attribute::StrictFP:
      case Attribute::UWTable:
      case Attribute::NoCfCheck:
        break;
      }

    newFunction->addFnAttr(Attr);
  }
  newFunction->getBasicBlockList().push_back(newRootNode);

  // Create an iterator to name all of the arguments we inserted.
  Function::arg_iterator AI = newFunction->arg_begin();

  // Rewrite all users of the inputs in the extracted region to use the
  // arguments (or appropriate addressing into struct) instead.
  for (unsigned i = 0, e = usedInputs.size(); i != e; ++i) {
    Value *RewriteVal;
    if (AggregateArgs) {
      Value *Idx[2];
      Idx[0] = Constant::getNullValue(Type::getInt32Ty(header->getContext()));
      Idx[1] = ConstantInt::get(Type::getInt32Ty(header->getContext()), i);
      TerminatorInst *TI = newFunction->begin()->getTerminator();
      GetElementPtrInst *GEP = GetElementPtrInst::Create(
          StructTy, &*AI, Idx, "gep_" + usedInputs[i]->getName(), TI);
      RewriteVal = new LoadInst(GEP, "loadgep_" + usedInputs[i]->getName(), TI);
    } else
      RewriteVal = &*AI++;

    std::vector<User *> Users(usedInputs[i]->user_begin(),
                              usedInputs[i]->user_end());
    for (User *use : Users)
      if (Instruction *inst = dyn_cast<Instruction>(use)) {
        auto found = std::find(CloneBlocks.begin(), CloneBlocks.end(),
                               inst->getParent());
        if (found != CloneBlocks.end())
          inst->replaceUsesOfWith(usedInputs[i], RewriteVal);
      }
  }

  AI = newFunction->arg_begin() + usedInputs.size();

  for (auto *v : inputs) {
    if (!isBidirectional(v)) {
      continue;
    }

    auto *outv = outputs[InputToOutputMap[v]];
    auto argIt = AI + InputToOutputMap[v];
    auto *ti = newFunction->begin()->getTerminator();

    // auto *ld = new LoadInst(&*argIt, "", ti);
    llvm::Value *ld = nullptr;

    if (!outv->getType()->isPointerTy()) {
      ld = new LoadInst(&*argIt, "", ti);
    } else {
      ld = &*argIt;
    }

    std::vector<User *> Users(v->user_begin(), v->user_end());
    for (User *use : Users)
      if (Instruction *inst = dyn_cast<Instruction>(use)) {
        auto found = std::find(CloneBlocks.begin(), CloneBlocks.end(),
                               inst->getParent());
        if (found != CloneBlocks.end())
          inst->replaceUsesOfWith(v, ld);
      }
  }

  // Set names for input and output arguments.
  if (!AggregateArgs) {
    AI = newFunction->arg_begin();
    for (unsigned i = 0, e = usedInputs.size(); i != e; ++i, ++AI)
      AI->setName(usedInputs[i]->getName());
    for (unsigned i = 0, e = outputs.size(); i != e; ++i, ++AI)
      AI->setName(outputs[i]->getName() + ".out");
  }

  auto &DL = newFunction->getParent()->getDataLayout();
  for (size_t i = 0; i < StackAllocas.size(); ++i) {
    auto *cloneAlloca = new llvm::AllocaInst(StackAllocas[i]->getType(),
                                             DL.getAllocaAddrSpace(), nullptr,
                                             "", newRootNode->getTerminator());
    if (StackAllocaInits.size() && StackAllocaInits[i]) {
      auto *storeClone = new llvm::StoreInst(StackAllocaInits[i], cloneAlloca,
                                             newRootNode->getTerminator());
    }

    auto *loadClone =
        new llvm::LoadInst(cloneAlloca, "strepl", newRootNode->getTerminator());

    std::vector<User *> Users(StackAllocas[i]->user_begin(),
                              StackAllocas[i]->user_end());
    for (User *use : Users) {
      if (Instruction *inst = dyn_cast<Instruction>(use)) {
        auto found = std::find(CloneBlocks.begin(), CloneBlocks.end(),
                               inst->getParent());
        if (found != CloneBlocks.end()) {
          inst->replaceUsesOfWith(StackAllocas[i], loadClone);
        }
      }
    }
  }

  // Set entry block successor to the clone blocks header
  auto *term = dyn_cast<BranchInst>(newRootNode->getTerminator());
  term->setSuccessor(0, header);

  // Divert all exiting branches to the single exit block of the new function
  newFunction->getBasicBlockList().push_back(newExitNode);
  auto *ret = ReturnInst::Create(newFunction->getContext(), newExitNode);

  for (auto *e : CloneBlocks) {
    auto *term = e->getTerminator();

    for (size_t i = 0; i < term->getNumSuccessors(); ++i) {
      if (!Blocks.count(term->getSuccessor(i))) {
        term->setSuccessor(i, newExitNode);
      }
    }
  }

  AI = newFunction->arg_begin() + usedInputs.size();

  for (auto *v : inputs) {
    if (!isBidirectional(v)) {
      continue;
    }

    auto *outv = outputs[InputToOutputMap[v]];
    auto argIt = AI + InputToOutputMap[v];
    auto *ti = newExitNode->getTerminator();

    if (!outv->getType()->isPointerTy()) {
      auto *st = new StoreInst(outv, &*argIt, false, ti);
    }
  }

  return newFunction;
}

void CodeExtractor::moveCodeToFunction(Function *newFunction) {
  Function *oldFunc = (*Blocks.begin())->getParent();
  Function::BasicBlockListType &oldBlocks = oldFunc->getBasicBlockList();
  Function::BasicBlockListType &newBlocks = newFunction->getBasicBlockList();

  for (BasicBlock *Block : Blocks) {
    // Delete the basic block from the old function, and the list of blocks
    oldBlocks.remove(Block);

    // Insert this basic block into the new function
    newBlocks.push_back(Block);
  }
}

void CodeExtractor::moveBlocksToFunction(ArrayRef<BasicBlock *> Blocks,
                                         Function *newFunction) {
  Function *oldFunc = (*Blocks.begin())->getParent();
  Function::BasicBlockListType &oldBlocks = oldFunc->getBasicBlockList();
  Function::BasicBlockListType &newBlocks = newFunction->getBasicBlockList();

  for (BasicBlock *Block : Blocks) {
    // Delete the basic block from the old function, and the list of blocks
    oldBlocks.remove(Block);

    // Insert this basic block into the new function
    newBlocks.push_back(Block);
  }
}

Function *CodeExtractor::cloneCodeRegion() {
  if (!isEligible())
    return nullptr;

  // Assumption: this is a single-entry code region, and the header is the first
  // block in the region.
  BasicBlock *header = *Blocks.begin();
  Function *oldFunction = header->getParent();

  // For functions with varargs, check that varargs handling is only done in the
  // outlined function, i.e vastart and vaend are only used in outlined blocks.
  if (AllowVarArgs && oldFunction->getFunctionType()->isVarArg()) {
    auto containsVarArgIntrinsic = [](Instruction &I) {
      if (const CallInst *CI = dyn_cast<CallInst>(&I))
        if (const Function *F = CI->getCalledFunction())
          return F->getIntrinsicID() == Intrinsic::vastart ||
                 F->getIntrinsicID() == Intrinsic::vaend;
      return false;
    };

    for (auto &BB : *oldFunction) {
      if (Blocks.count(&BB))
        continue;
      if (llvm::any_of(BB, containsVarArgIntrinsic))
        return nullptr;
    }
  }

  // If we have to split PHI nodes or the entry block, do so now.
  severSplitPHINodes(header);

  // If we have any return instructions in the region, split those blocks so
  // that the return is not in the region.
  splitReturnBlocks();

  // The new function needs a root node because other nodes can branch to the
  // head of the region, but the entry node of a function cannot have preds.
  BasicBlock *newFuncRoot =
      BasicBlock::Create(header->getContext(), "newFuncRoot");
  auto *BranchI = BranchInst::Create(header);
  // If the original function has debug info, we have to add a debug location
  // to the new branch instruction from the artificial entry block.
  // We use the debug location of the first instruction in the extracted
  // blocks, as there is no other equivalent line in the source code.
  if (oldFunction->getSubprogram()) {
    any_of(Blocks, [&BranchI](const BasicBlock *BB) {
      return any_of(*BB, [&BranchI](const Instruction &I) {
        if (!I.getDebugLoc())
          return false;
        BranchI->setDebugLoc(I.getDebugLoc());
        return true;
      });
    });
  }
  newFuncRoot->getInstList().push_back(BranchI);

  for (auto *b : Blocks) {
    CloneBlocks.push_back(CloneBasicBlock(b, VMap, ".clone", oldFunction));
    VMap[b] = CloneBlocks.back();
  }
  auto *cloneHeader = cast_or_null<llvm::BasicBlock>(VMap[header]);

  auto *newFuncExit = BasicBlock::Create(header->getContext(), "exit");

  if (FlattenArrayAccesses) {
    for (auto &e : Accesses->Accesses) {
      auto *ptr = e.getPointerOperand();

      llvm::SmallPtrSet<llvm::GetElementPtrInst *, 8> geps;
      if (ptr && llvm::isa<llvm::GetElementPtrInst>(ptr)) {
        auto &cgep = *VMap[ptr];
        geps.insert(llvm::dyn_cast<llvm::GetElementPtrInst>(&cgep));
      }

      for (auto *gep : geps) {
        FlattenMultiDimArrayIndices(gep);
      }
    }
  }

  // Construct new function based on inputs/outputs & add allocas for all defs.
  Function *newFunction =
      cloneFunction(Inputs, Outputs, cloneHeader, newFuncRoot, newFuncExit,
                    oldFunction, oldFunction->getParent());

  for (auto &inst : *newFuncExit) {
    if (inst.isTerminator()) {
      break;
    }

    RemapInstruction(&inst, VMap,
                     RF_NoModuleLevelChanges | RF_IgnoreMissingLocals);
  }

  remapCloneBlocks();

  // auto &DL = newFunction->getParent()->getDataLayout();
  // for (size_t i = 0; i < StackAllocas.size(); ++i) {
  // llvm::dbgs() << "REPL " << *StackAllocas[i] << '\n';
  // auto *cloneAlloca = new llvm::AllocaInst(StackAllocas[i]->getType(),
  // DL.getAllocaAddrSpace(), nullptr,
  //"", newFuncRoot->getTerminator());
  // if (StackAllocaInits.size() && StackAllocaInits[i]) {
  // auto *storeClone = new llvm::StoreInst(StackAllocaInits[i], cloneAlloca,
  // newFuncRoot->getTerminator());
  //}

  // auto *loadClone =
  // new llvm::LoadInst(cloneAlloca, "conco", newFuncRoot->getTerminator());

  // std::vector<User *> Users(StackAllocas[i]->user_begin(),
  // StackAllocas[i]->user_end());
  // for (User *use : Users) {
  // if (Instruction *inst = dyn_cast<Instruction>(use)) {
  // llvm::dbgs() << "USE " << *StackAllocas[i] << '\n';
  // auto found = std::find(CloneBlocks.begin(), CloneBlocks.end(),
  // inst->getParent());
  // if (found != CloneBlocks.end()) {
  // llvm::dbgs() << "REPL2 " << *StackAllocas[i] << '\n';
  // inst->replaceUsesOfWith(StackAllocas[i], loadClone);
  //}
  //}
  //}
  //}

  moveBlocksToFunction(CloneBlocks, newFunction);

  // Propagate personality info to the new function if there is one.
  if (oldFunction->hasPersonalityFn())
    newFunction->setPersonalityFn(oldFunction->getPersonalityFn());

  // Loop over all of the PHI nodes in the header block, and change any
  // references to the old incoming edge to be the new incoming edge.
  for (BasicBlock::iterator I = cloneHeader->begin(); isa<PHINode>(I); ++I) {
    PHINode *PN = cast<PHINode>(I);
    for (unsigned i = 0, e = PN->getNumIncomingValues(); i != e; ++i) {
      auto *b = PN->getIncomingBlock(i);
      if (b->getParent() != newFunction) {
        PN->setIncomingBlock(i, newFuncRoot);
      }
    }
  }

  InstructionEraser ie;
  ie.visit(newFunction);
  ie.process();

  LLVM_DEBUG(if (VerifyOption && verifyFunction(*newFunction, &llvm::dbgs())) {
    newFunction->dump();
    report_fatal_error("verifyFunction failed!");
  });

  return newFunction;
}

