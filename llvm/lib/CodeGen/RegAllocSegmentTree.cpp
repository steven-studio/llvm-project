//===- RegAllocSegmentTree.cpp - Segment Tree Register Allocator -*- C++ -*-===//
//
// This file implements a register allocator using segment tree (demo version).
//
//===----------------------------------------------------------------------===//

#include "RegAllocSegmentTree.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/RegAllocRegistry.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/LiveIntervals.h"
#include "llvm/CodeGen/TargetRegisterInfo.h"
#include "llvm/CodeGen/MachineDominators.h"
#include "llvm/CodeGen/LiveStacks.h"
#include "llvm/CodeGen/LazyMachineBlockFrequencyInfo.h"
#include "llvm/Pass.h"
#include "llvm/CodeGen/LiveRegMatrix.h"
#include "llvm/CodeGen/VirtRegMap.h"
#include "llvm/CodeGen/Spiller.h"

#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"   // ← for report_fatal_error
#include "llvm/Support/raw_ostream.h"

#include <algorithm>
#include <queue>
#include <cassert>

using namespace llvm;

#define DEBUG_TYPE "regalloc-segmenttree"

// 線段樹節點
struct SegmentTreeNode {
  unsigned left, right;
  unsigned maxFree; // 區間內最大可用暫存器數
  SegmentTreeNode *lchild, *rchild;
  SegmentTreeNode(unsigned l, unsigned r)
      : left(l), right(r), maxFree(r - l + 1), lchild(nullptr), rchild(nullptr) {}
};

// 線段樹本體
class SegmentTree {
  SegmentTreeNode *root;
public:
  SegmentTree(unsigned n) { root = build(0, n - 1); }
  ~SegmentTree() { destroy(root); }
  // 建立線段樹
  SegmentTreeNode* build(unsigned l, unsigned r) {
    SegmentTreeNode *node = new SegmentTreeNode(l, r);
    if (l == r) return node;
    unsigned mid = (l + r) / 2;
    node->lchild = build(l, mid);
    node->rchild = build(mid + 1, r);
    return node;
  }
  void destroy(SegmentTreeNode *node) {
    if (!node) return;
    destroy(node->lchild);
    destroy(node->rchild);
    delete node;
  }
  // 查詢區間是否有可用暫存器
  bool query(SegmentTreeNode *node, unsigned l, unsigned r) {
    if (!node || node->maxFree == 0 || r < node->left || l > node->right)
      return false;
    if (l <= node->left && node->right <= r)
      return node->maxFree > 0;
    return query(node->lchild, l, r) || query(node->rchild, l, r);
  }

  // 分配暫存器（標記區間已用）
  void allocate(SegmentTreeNode *node, unsigned pos) {
    if (!node || node->maxFree == 0) return;
    if (node->left == node->right && node->left == pos) {
      node->maxFree = 0;
      return;
    }
    unsigned mid = (node->left + node->right) / 2;
    if (pos <= mid)
      allocate(node->lchild, pos);
    else
      allocate(node->rchild, pos);
    node->maxFree = std::max(node->lchild->maxFree, node->rchild->maxFree);
  }

  // 釋放暫存器（標記區間可用）
  void release(SegmentTreeNode *node, unsigned pos) {
    if (!node) return;
    if (node->left == node->right && node->left == pos) {
      node->maxFree = 1;
      return;
    }
    unsigned mid = (node->left + node->right) / 2;
    if (pos <= mid)
      release(node->lchild, pos);
    else
      release(node->rchild, pos);
    node->maxFree = std::max(node->lchild->maxFree, node->rchild->maxFree);
  }

  // 封裝對外介面
  bool hasFree(unsigned l, unsigned r) { return query(root, l, r); }
  void alloc(unsigned pos) { allocate(root, pos); }
  void free(unsigned pos) { release(root, pos); }
};

//-----------------
// 核心改動：header 宣告的 RASegmentTree class 實作
//-----------------

char RASegmentTree::ID = 0;

RASegmentTree::RASegmentTree(const RegAllocFilterFunc F)
  : MachineFunctionPass(ID)
{
  // 可以初始化變數
  llvm::errs() << "[DEBUG] RASegmentTree constructor called\n";
}

void RASegmentTree::getAnalysisUsage(AnalysisUsage &AU) const {
  // 需要的分析 pass
  // AU.addRequired<LiveIntervals>();
  MachineFunctionPass::getAnalysisUsage(AU);
}

void RASegmentTree::releaseMemory() {
  // 釋放內部資源
}

bool RASegmentTree::runOnMachineFunction(MachineFunction &MF) {
  LLVM_DEBUG(dbgs() << "Running Segment Tree Register Allocator on "
                    << MF.getName() << "\n");
  errs() << "[RegAllocSegmentTree] Allocating registers for function: "
         << MF.getName() << "\n";
  const TargetRegisterInfo *TRI = MF.getSubtarget().getRegisterInfo();
  const TargetRegisterClass *MaxRC = nullptr;
  unsigned MaxPhys = 0;
  for (auto *RCIt : TRI->regclasses()) {
    if (RCIt->getNumRegs() > MaxPhys)
      MaxRC = RCIt, MaxPhys = RCIt->getNumRegs();
  }
  unsigned NumPhysRegs = MaxRC ? MaxRC->getNumRegs() : 32;
  SegmentTree segTree(NumPhysRegs);
  // ... 進一步實作 allocation logic
  return false; // No modification for now
}


// ====== 這裡直接補齊純虛擬函式的 override ======

Spiller &RASegmentTree::spiller() {
  report_fatal_error("RASegmentTree::spiller() not implemented");
}

void RASegmentTree::enqueueImpl(const LiveInterval *LI) {
  // 直接忽略，或存進一個 queue（如果你要真的實現分配邏輯，這裡要有 queue）
  // 最小可用實作可為空
}

const LiveInterval *RASegmentTree::dequeue() {
  // 返回 nullptr，代表 queue 已空，LLVM 流程不會 crash
  return nullptr;
}

MCRegister RASegmentTree::selectOrSplit(const LiveInterval &VirtReg,
                                        SmallVectorImpl<Register> &SplitVRegs) {
  // 返回 0，代表沒分配到暫存器，LLVM 會自動溢出 spill
  return 0;
}

//-----------------
// 導出 factory function（對應 header 宣告）
//-----------------
FunctionPass *llvm::createSegmentTreeRegisterAllocator() {
  llvm::errs() << "[DEBUG] createSegmentTreeRegisterAllocator called\n";
  return new RASegmentTree();
}

//-----------------
// 註冊 allocator
//-----------------
static RegisterRegAlloc segmentTreeRegAlloc(
    "segmenttree",
    "Segment Tree Register Allocator",
    llvm::createSegmentTreeRegisterAllocator
);

// 強制初始化函數
namespace llvm {
void LLVMInitializeSegmentTreeRegisterAllocator() {
  llvm::errs() << "[DEBUG] LLVMInitializeSegmentTreeRegisterAllocator called\n";
  // 強制引用靜態變量，確保不被優化掉
  (void)&segmentTreeRegAlloc;
}
}

// // 在模塊載入時自動調用
// namespace {
//   struct ForceInit {
//     ForceInit() {
//       LLVMInitializeSegmentTreeRegisterAllocator();
//     }
//   };
//   static ForceInit forceInit;
// }
