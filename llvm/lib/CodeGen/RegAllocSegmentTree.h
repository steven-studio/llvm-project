#ifndef LLVM_CODEGEN_REGALLOCSEGMENTTREE_H
#define LLVM_CODEGEN_REGALLOCSEGMENTTREE_H

#include "RegAllocBase.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
// ...其他需要的 include

namespace llvm {

class LLVM_LIBRARY_VISIBILITY RASegmentTree : public MachineFunctionPass, public RegAllocBase {
  // segment tree 狀態欄位
  // 其他 allocator 需要的變數

public:
  static char ID;
  RASegmentTree(const RegAllocFilterFunc F = nullptr);

  StringRef getPassName() const override { return "Segment Tree Register Allocator"; }
  void getAnalysisUsage(AnalysisUsage &AU) const override;
  void releaseMemory() override;

  // 其他 override, 如 runOnMachineFunction 等
  bool runOnMachineFunction(MachineFunction &mf) override;

};
FunctionPass *createSegmentTreeRegisterAllocator();
} // namespace llvm
#endif

