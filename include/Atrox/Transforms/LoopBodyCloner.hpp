//
//
//

#include "Atrox/Config.hpp"

#include "Atrox/Transforms/Utils/CodeExtractor.hpp"

#include "Atrox/Support/IR/ArgSpec.hpp"

#include "Atrox/Support/IR/ArgUtils.hpp"

#include "Atrox/Analysis/MemoryAccessInfo.hpp"

#include "Atrox/Exchange/Info.hpp"

#include "IteratorRecognition/Analysis/IteratorRecognition.hpp"

#include "llvm/IR/Module.h"
// using llvm::Module

#include "llvm/IR/Function.h"
// using llvm::Function

#include "llvm/IR/Dominators.h"
// using llvm::DominatorTree

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
      llvm::AAResults *AA = nullptr) {
    bool hasChanged = false;

    llvm::SmallVector<llvm::BasicBlock *, 32> blocks;
    Selector.getBlocks(L, blocks);

    if (!blocks.empty()) {
      atrox::CodeExtractor ce{blocks};

      llvm::SetVector<llvm::Value *> inputs, outputs, sinks;
      ce.findInputsOutputs(inputs, outputs, sinks);

      CodeExtractor::InputToOutputMapTy ioMap;
      CodeExtractor::OutputToInputMapTy oiMap;
      ce.mapInputsOutputs(inputs, outputs, ioMap, oiMap);

      llvm::SmallVector<ArgDirection, 16> argDirs;
      MemoryAccessInfo mai{blocks, AA};
      GenerateArgDirection(inputs, outputs, oiMap, argDirs, &mai);

#if !defined(NDEBUG)
      LLVM_DEBUG({
        llvm::dbgs() << "inputs: " << inputs.size() << "\n";
        llvm::dbgs() << "outputs: " << outputs.size() << "\n";
        for (llvm::Value *value : inputs)
          llvm::dbgs() << "    value used in func: " << *value << "\n";
        for (llvm::Value *output : outputs)
          llvm::dbgs() << "value used out of func: " << *output << "\n";
      });

      for (const auto &e : ioMap) {
        LLVM_DEBUG({
          llvm::dbgs() << "in: " << *e.first << " -> out: " << e.second << "\n";
        });
      }

      for (const auto &e : oiMap) {
        LLVM_DEBUG({
          llvm::dbgs() << "out: " << *e.first << " -> in: " << *e.second
                       << "\n";
        });
      }
#endif // !defined(NDEBUG)

      llvm::SmallVector<bool, 16> argIteratorVariance;

      if (!ITRInfo) {
        argIteratorVariance.resize(argDirs.size(), false);
      } else {
        auto infoOrError = ITRInfo.getValue()->getIteratorInfoFor(&L);
        if (!infoOrError) {
        }
        auto info = *infoOrError;

        GenerateArgIteratorVariance(inputs, outputs, info, argIteratorVariance);
      }

      auto *extractedFunc = ce.cloneCodeRegion();
      hasChanged |= extractedFunc ? true : false;

      if (ShouldStoreInfo) {
        std::vector<ArgSpec> specs;

        for (size_t i = 0; i < argDirs.size(); ++i) {
          specs.push_back({argDirs[i], argIteratorVariance[i]});
        }

        StoreInfo.push_back({extractedFunc, &L, specs});
      }
    } else {
      LLVM_DEBUG(llvm::dbgs()
                 << "Skipping loop because no blocks were selected.\n");
    }

    return hasChanged;
  }

  template <typename T>
  bool cloneLoops(
      llvm::LoopInfo &LI, T &Selector,
      llvm::Optional<iteratorrecognition::IteratorRecognitionInfo *> ITRInfo,
      llvm::AAResults *AA = nullptr) {
    bool hasChanged = false;

    for (auto *curLoop : LI) {
      hasChanged |= cloneLoop(*curLoop, LI, Selector, ITRInfo, AA);
    }

    return hasChanged;
  }
};

} // namespace atrox

#undef DEBUG_TYPE

