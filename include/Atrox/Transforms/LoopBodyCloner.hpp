//
//
//

#include "Atrox/Config.hpp"

#include "Atrox/Transforms/Utils/CodeExtractor.hpp"

#include "Atrox/Support/IR/ArgSpec.hpp"

#include "Atrox/Support/IR/ArgUtils.hpp"

#include "Atrox/Support/IR/GeneralUtils.hpp"

#include "Atrox/Analysis/LoopBoundsAnalyzer.hpp"

#include "Atrox/Analysis/MemoryAccessInfo.hpp"

#include "Atrox/Exchange/Info.hpp"

#include "IteratorRecognition/Analysis/IteratorRecognition.hpp"

#include "private/PassCommandLineOptions.hpp"

#include "llvm/IR/Module.h"
// using llvm::Module

#include "llvm/IR/Function.h"
// using llvm::Function

#include "llvm/IR/Dominators.h"
// using llvm::DominatorTree

#include "llvm/Analysis/ScalarEvolution.h"
// using llvm::ScalarEvolution

#include "llvm/Analysis/ScalarEvolutionExpressions.h"
// using llvm::SCEVConstant

#include "llvm/Analysis/LoopInfo.h"
// using llvm::LoopInfo
// using llvm::Loop

#include "llvm/Analysis/AliasAnalysis.h"
// using llvm::AAResults

#include "llvm/ADT/SmallVector.h"
// using llvm::SmallVector

#include "llvm/ADT/SetVector.h"
// using llvm::SetVector

#include "llvm/ADT/Optional.h"
// uaing llvm::Optional

#include "llvm/Support/Debug.h"
// using DEBUG macro
// using llvm::dbgs

#include <algorithm>
// using std::count_if

#include <cassert>
// using assert

#define DEBUG_TYPE "atrox-loop-body-clone"

namespace atrox {

class LoopBodyCloner {
  llvm::Module *TargetModule;
  bool ShouldStoreInfo;
  llvm::SmallVector<FunctionArgSpec, 32> StoreInfo;

public:
  explicit LoopBodyCloner(llvm::Module &CurM, bool _ShouldStoreInfo = false)
      : TargetModule(&CurM), ShouldStoreInfo(_ShouldStoreInfo) {}

  auto &getInfo() const { return StoreInfo; }

  template <typename T>
  bool cloneLoop(
      llvm::Loop &L, llvm::LoopInfo &LI, T &Selector,
      llvm::Optional<iteratorrecognition::IteratorRecognitionInfo *> ITRInfo,
      llvm::Optional<iteratorrecognition::DispositionTracker> IDT,
      LoopBoundsAnalyzer &LBA, llvm::AAResults *AA = nullptr) {
    llvm::SmallVector<llvm::BasicBlock *, 32> blocks;
    Selector.getBlocks(L, blocks);

    if (blocks.empty()) {
      LLVM_DEBUG(llvm::dbgs()
                 << "skipping loop because no blocks were selected.\n");

      return false;
    }

    if (AtroxSkipCalls) {
      CallDetector cd;
      cd.visit(blocks.begin(), blocks.end());

      if (cd) {
        LLVM_DEBUG(llvm::dbgs()
                   << "skipping loop because it contains calls.\n");
        return false;
      }
    }

    bool hasChanged = false;
    atrox::CodeExtractor ce{blocks};

    llvm::SetVector<llvm::Value *> inputs, outputs, sinks;
    ce.findInputsOutputs(inputs, outputs, sinks);
    ce.findGlobalInputsOutputs(inputs, outputs);

    CodeExtractor::InputToOutputMapTy ioMap;
    CodeExtractor::OutputToInputMapTy oiMap;
    ce.mapInputsOutputs(inputs, outputs, ioMap, oiMap);

    if (ITRInfo) {
      auto infoOrError = ITRInfo.getValue()->getIteratorInfoFor(&L);
      if (!infoOrError) {
        LLVM_DEBUG(llvm::dbgs() << "No iterator info for loop\n";);
      }

      auto info = *infoOrError;

      auto n = std::count_if(inputs.begin(), inputs.end(), [&info](auto *e) {
        if (const auto *i = llvm::dyn_cast<const llvm::Instruction>(e)) {
          return info.isIterator(i);
        }

        return false;
      });

      if (n != 1) {
        LLVM_DEBUG(llvm::dbgs()
                   << "Cannot handle " << n << " input iterators!\n");

        return false;
      }

      ReorderInputs(inputs, info);
    }

    llvm::SetVector<llvm::Value *> toStackAllocate;
    llvm::SmallVector<llvm::Value *, 8> toStackAllocateInit;
    {
      llvm::SmallPtrSet<llvm::BasicBlock *, 8> scopeBlocks{blocks.begin(),
                                                           blocks.end()};
      auto evalLBA = LBA;
      evalLBA.evaluate(0, 5, &L);

      for (auto *v : inputs) {
        auto isCondUse =
            LBA.isValueUsedOnlyInLoopNestConditions(v, &L, scopeBlocks);
        auto isOuterIndVar = LBA.isValueOuterLoopInductionVariable(v, &L);
        auto isInnerIndVar = LBA.isValueInnerLoopInductionVariable(v, &L);

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

          toStackAllocateInit.push_back(nullptr);
          toStackAllocate.insert(v);
        }

        if (isOuterIndVar) {
          auto lbInfoOrErr = evalLBA.getInfo(v);
          if (!lbInfoOrErr) {
            LLVM_DEBUG(llvm::dbgs() << "Missing loop iteration space info!\n";);
            // assert?
          }
          auto lbInfo = *lbInfoOrErr;

          auto *start =
              llvm::dyn_cast_or_null<llvm::SCEVConstant>(lbInfo.Start);
          llvm::ConstantInt *initVal = nullptr;
          if (start) {
            // FIXME maybe checkthat the induction var is an integer type before
            // casting
            initVal = llvm::dyn_cast<llvm::ConstantInt>(
                llvm::ConstantInt::get(lbInfo.InductionVariable->getType(),
                                       start->getValue()->getZExtValue()));
          } else {
            // TODO what happens here if the value is used in the loop in a
            // division or a subtraction that results in a negative number?
            uint64_t val = 0;
            initVal = llvm::dyn_cast<llvm::ConstantInt>(llvm::ConstantInt::get(
                lbInfo.InductionVariable->getType(), val));

            LLVM_DEBUG(llvm::dbgs()
                           << "outer induction variable: "
                           << *lbInfo.InductionVariable
                           << " does not have a constant start value: "
                           << *lbInfo.Start << " set to: " << val << '\n';);
          }

          toStackAllocateInit.push_back(initVal);
          toStackAllocate.insert(v);
        }

        if (isCondUse) {
          auto *initVal = llvm::ConstantInt::get(v->getType(), 5);
          toStackAllocateInit.push_back(initVal);
          toStackAllocate.insert(v);
        }
      }

      for (auto *e : toStackAllocate) {
        inputs.remove(e);
      }
    }

    MemoryAccessInfo mai{blocks, AA};
    MemAccInstVisitor accesses;
    accesses.visit(blocks.begin(), blocks.end());

#if !defined(NDEBUG)
    LLVM_DEBUG({
      llvm::dbgs() << "inputs: " << inputs.size() << "\n";
      llvm::dbgs() << "outputs: " << outputs.size() << "\n";
      for (llvm::Value *e : inputs) {
        llvm::dbgs() << "    value used in func: " << *e << "\n";
      }
      for (llvm::Value *e : outputs) {
        llvm::dbgs() << "value used out of func: " << *e << "\n";
      }
      for (llvm::Value *e : toStackAllocate) {
        llvm::dbgs() << " to be stack allocated: " << *e << "\n";
      }
    });

    for (const auto &e : ioMap) {
      LLVM_DEBUG({
        llvm::dbgs() << "in: " << *e.first << " -> out: " << e.second << "\n";
      });
    }

    for (const auto &e : oiMap) {
      LLVM_DEBUG({
        llvm::dbgs() << "out: " << *e.first << " -> in: " << *e.second << "\n";
      });
    }
#endif // !defined(NDEBUG)

    ce.setInputs(inputs);
    ce.setOutputs(outputs);
    ce.setStackAllocas(toStackAllocate, &toStackAllocateInit);
    ce.setAccesses(&accesses);
    auto *extractedFunc = ce.cloneCodeRegion(false);

    if (extractedFunc) {
      hasChanged |= true;

      // remove bidirectional inputs
      llvm::SmallVector<llvm::Value *, 4> toRemove;
      for (auto *e : inputs) {
        if (IsBidirectional(e, oiMap)) {
          toRemove.push_back(e);
        }
      }

      for (auto *e : toRemove) {
        inputs.remove(e);
      }

      llvm::SmallVector<ArgDirection, 16> argDirs;
      GenerateArgDirection(inputs, outputs, oiMap, argDirs, &mai);

      llvm::SmallVector<bool, 16> argIteratorVariance;

      if (!IDT) {
        argIteratorVariance.resize(argDirs.size(), false);
      } else {
        GenerateArgIteratorVariance(L, inputs, outputs, *IDT,
                                    argIteratorVariance);
      }

      if (ShouldStoreInfo) {
        std::vector<ArgSpec> specs;

        assert(extractedFunc->arg_size() == argDirs.size() &&
               "Arguments and their specs must be the same number!");

        auto argIt = extractedFunc->arg_begin();
        for (size_t i = 0; i < argDirs.size(); ++i) {
          specs.push_back(
              {argIt->getName(), argDirs[i], argIteratorVariance[i]});
          ++argIt;
        }

        StoreInfo.push_back({extractedFunc, &L, specs});
      }
    }

    return hasChanged;
  }

  template <typename T>
  bool cloneLoops(llvm::LoopInfo &LI, T &Selector,
                  llvm::Optional<iteratorrecognition::IteratorRecognitionInfo *>
                      ITRInfoOrEmpty,
                  llvm::ScalarEvolution *SE = nullptr,
                  llvm::AAResults *AA = nullptr) {
    bool hasChanged = false;

    auto loops = LI.getLoopsInPreorder();
    LoopBoundsAnalyzer lba{LI, *SE};

    llvm::Optional<iteratorrecognition::DispositionTracker> idtOrEmpty;
    if (auto *ITRInfo = *ITRInfoOrEmpty) {
      idtOrEmpty = iteratorrecognition::DispositionTracker{*ITRInfo};
    }

    for (auto *curLoop : loops) {
      lba.analyze(curLoop);

      LLVM_DEBUG(llvm::dbgs() << "processing loop: "
                              << curLoop->getHeader()->getName() << '\n';);
      hasChanged |= cloneLoop(*curLoop, LI, Selector, ITRInfoOrEmpty,
                              idtOrEmpty, lba, AA);
    }

    return hasChanged;
  }
};

} // namespace atrox

#undef DEBUG_TYPE

