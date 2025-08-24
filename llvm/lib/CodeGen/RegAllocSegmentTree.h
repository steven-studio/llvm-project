#ifndef LLVM_CODEGEN_REGALLOCSEGMENTTREE_H
#define LLVM_CODEGEN_REGALLOCSEGMENTTREE_H

#include "RegAllocBase.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
// ...其他需要的 include
#include "llvm/ADT/SmallVector.h"     // SmallVectorImpl
#include "llvm/CodeGen/Register.h"    // Register
#include "llvm/MC/MCRegister.h"       // MCRegister

#include <memory>
#include <queue>

namespace llvm {

class MachineFunction;
class LiveIntervals;
class LiveInterval;
class VirtRegMap;
class Spiller;

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

protected:
  // === RegAllocBase 的純虛擬函式：必須宣告並覆寫 ===
  Spiller &spiller() override;
  void enqueueImpl(const LiveInterval *LI) override;
  const LiveInterval *dequeue() override;
  MCRegister selectOrSplit(const LiveInterval &VirtReg,
                           SmallVectorImpl<Register> &SplitVRegs) override;

private:
  MachineFunction *MFPtr = nullptr;
  LiveIntervals   *LISPtr = nullptr;
  std::unique_ptr<VirtRegMap> LocalVRM;
  std::unique_ptr<Spiller>    TheSpiller;
  std::queue<const LiveInterval*> WorkQ;
};
FunctionPass *createSegmentTreeRegisterAllocator();
} // namespace llvm
#endif

