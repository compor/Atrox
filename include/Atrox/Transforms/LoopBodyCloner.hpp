//
//
//

#include "Atrox/Config.hpp"

#include "Atrox/Transforms/Utils/CodeExtractor.hpp"

#include "Atrox/Support/IR/ArgSpec.hpp"

#include "llvm/IR/Module.h"
// using llvm::Module

#include "llvm/IR/Function.h"
// using llvm::Function

#include "llvm/IR/Dominators.h"
// using llvm::DominatorTree

#include "llvm/Analysis/LoopInfo.h"
// using llvm::LoopInfo
// using llvm::Loop

#include "llvm/ADT/SmallVector.h"
// using llvm::SmallVector

#include "llvm/ADT/SetVector.h"
// using llvm::SetVector

#include "llvm/ADT/DenseMap.h"
// using llvm::DenseMap

#include "llvm/Support/Debug.h"
// using DEBUG macro
// using llvm::dbgs

#define DEBUG_TYPE "atrox-loop-body-clone"

namespace atrox {

class LoopBodyCloner {
  llvm::Module *TargetModule;
  bool StoreInfo;
  llvm::DenseMap<llvm::Function *, llvm::SmallVector<ArgSpec, 8>> CloneInfo;

public:
  explicit LoopBodyCloner(llvm::Module &CurM, bool ShouldStoreInfo = false)
      : TargetModule(&CurM), StoreInfo(ShouldStoreInfo) {}

  template <typename T>
  bool cloneLoop(llvm::Loop &L, llvm::LoopInfo &LI, T &Selector) {
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
      ce.generateArgDirection(inputs, outputs, oiMap, argDirs);

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

      auto *extractedFunc = ce.cloneCodeRegion();
      hasChanged |= extractedFunc ? true : false;

      if (StoreInfo) {
        decltype(CloneInfo)::mapped_type s;
        for (auto &e : argDirs) {
          s.push_back({e, false});
        }
        CloneInfo[extractedFunc] = s;
      }
    } else {
      LLVM_DEBUG(llvm::dbgs()
                 << "Skipping loop because no blocks were selected.\n");
    }

    return hasChanged;
  }

  template <typename T> bool cloneLoops(llvm::LoopInfo &LI, T &Selector) {
    bool hasChanged = false;

    for (auto *curLoop : LI) {
      hasChanged |= cloneLoop(*curLoop, LI, Selector);
    }

    return hasChanged;
  }
};

} // namespace atrox

#undef DEBUG_TYPE

