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

#include "llvm/Analysis/LoopIterator.h"

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

    // reorder blocks
    llvm::LoopBlocksRPO rpo(&L);
    rpo.perform(&LI);
    llvm::SmallVector<llvm::BasicBlock *, 32> initial{blocks};
    blocks.clear();
    for (auto *bb : rpo) {
      if (initial.end() != std::find(initial.begin(), initial.end(), bb)) {
        blocks.push_back(bb);
      }
    }
    initial.clear();

    if (AtroxSkipCalls) {
      CallDetector cd{TargetModule};
      cd.visit(blocks.begin(), blocks.end());

      if (cd) {
        LLVM_DEBUG(llvm::dbgs()
                   << "skipping loop because it contains calls.\n");
        return false;
      }
    }

    bool hasChanged = false;
    iteratorrecognition::IteratorInfo info;

    if (ITRInfo) {
      auto infoOrError = ITRInfo.getValue()->getIteratorInfoFor(&L);
      if (!infoOrError) {
        LLVM_DEBUG(llvm::dbgs() << "No iterator info for loop\n";);
      }

      info = *infoOrError;
    }

    atrox::CodeExtractor ce{blocks, L, &info, &LBA};
    ce.prepare();

    if (info) {
      auto n = std::count_if(
          ce.getPureInputs().begin(), ce.getPureInputs().end(),
          [&info](auto *e) {
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
    }

    MemoryAccessInfo mai{blocks, AA};
    MemAccInstVisitor accesses;
    accesses.visit(blocks.begin(), blocks.end());

#if !defined(NDEBUG)
    LLVM_DEBUG({
      llvm::dbgs() << "inputs: " << ce.getPureInputs().size()
                   << "\noutputs: " << ce.getOutputs().size() << "\n";
      for (llvm::Value *e : ce.getPureInputs()) {
        llvm::dbgs() << "    value used in func: " << *e << "\n";
      }
      for (llvm::Value *e : ce.getOutputs()) {
        llvm::dbgs() << "value used out of func: " << *e << "\n";
      }
    });
#endif // !defined(NDEBUG)

    ce.setAccesses(&accesses);
    auto *extractedFunc = ce.cloneCodeRegion();

    if (extractedFunc) {
      hasChanged |= true;

      llvm::SmallVector<ArgDirection, 16> argDirs;
      GenerateArgDirection(ce.getPureInputs(), ce.getOutputs(), argDirs, &mai);

      llvm::SmallVector<bool, 16> argIteratorVariance;

      if (!IDT) {
        argIteratorVariance.resize(argDirs.size(), false);
      } else {
        GenerateArgIteratorVariance(L, ce.getPureInputs(), ce.getOutputs(),
                                    *IDT, argIteratorVariance);
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

