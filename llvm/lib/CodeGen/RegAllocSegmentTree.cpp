//===- RegAllocSegmentTree.cpp - Segment Tree Register Allocator -*- C++ -*-===//
//
// This file implements a register allocator using segment tree (demo version).
//
//===----------------------------------------------------------------------===//

#include "RegAllocSegmentTree.h"
#include "llvm/ADT/DenseMap.h"
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
  AU.addRequiredID(LiveIntervalsID);                   // LIS（用 ID）
  AU.addRequiredID(LiveStacksID);                      // LSS（用 ID）
  AU.addRequired<MachineDominatorTreeWrapperPass>();   // MDT：WrapperPass
  // AU.addRequired<MachineLoopInfo>();
  // AU.addRequired<VirtRegMap>();
  // AU.addRequired<LiveRegMatrix>();
  // AU.addRequired<RegisterClassInfo>();
  AU.addRequired<LazyMachineBlockFrequencyInfoPass>();     // 從 Lazy 取 MBFI
  // ✘ AU.addRequired<LiveRegMatrix>();   // ← 刪掉
  MachineFunctionPass::getAnalysisUsage(AU);
}

void RASegmentTree::releaseMemory() {
  // 釋放內部資源
}

bool RASegmentTree::runOnMachineFunction(MachineFunction &MF) {
  MFPtr = &MF;

  auto &LIS = getAnalysisID<LiveIntervals>(&LiveIntervalsID);
  LISPtr = &LIS;

  LocalVRM = std::make_unique<VirtRegMap>();

  MachineRegisterInfo &MRI = MF.getRegInfo();

  // 可能為 0（例如函式沒有產生 vreg），做個保護
  unsigned NumVRegs = MRI.getNumVirtRegs();
  if (NumVRegs == 0) NumVRegs = 1;

  SegmentTree ST(NumVRegs);  // 範圍索引對應 vreg 索引 [0, NumVRegs-1]

  const TargetRegisterInfo *TRI = MF.getSubtarget().getRegisterInfo();

  // 1) 所有實體暫存器的總數（包含保留/不可分配）
  unsigned NumPhysRegsTotal = TRI->getNumRegs();

  // 1) 建立「所有可分配的實體暫存器」池（先不分 RC；挑到時再檢 RC）
  //    也可以用 TRI->getAllocatableSet(MF) 之類 API；這裡簡化成從 regclasses 蒐集。
  SmallVector<MCRegister, 64> AllPhys;
  for (auto *RC : TRI->regclasses())
    for (MCRegister PR : *RC)
      AllPhys.push_back(PR);

  // 2) 每個 phys reg 維護一組它目前已被指派的 vreg（用來做 LI 重疊檢查）
  DenseMap<MCRegister, SmallVector<Register, 8>> PhysAssigned;

  // 3) 遍歷函式裡的每個 vreg，嘗試指派一個 phys
  // 遍歷所有 vreg（通用、安全）
  for (unsigned I = 0, E = MRI.getNumVirtRegs(); I != E; ++I) {
    Register V = Register::index2VirtReg(I);
    if (MRI.reg_nodbg_empty(V)) continue;

    const TargetRegisterClass *RC = MRI.getRegClass(V);
    LiveInterval &LI = LIS.getInterval(V);

    bool Assigned = false;
    for (MCRegister PR : AllPhys) {
      // 只檢查 RC 相容性
      if (!RC->contains(PR)) continue;
      // if (!AllocatableSet.test(PR)) continue;           // 可分配/非保留
      // if (isLiveInOutOrFixed(PR, MF)) continue;         // live-in/out、固定寄存器
      // if (callClobbers(PR, LI, MF)) continue;           // regmask / call clobber
      // if (violatesTiedOperand(PR, V, MI, TRI)) continue;// two-address / tied 約束
      // if (subregOrLaneConflict(PR, V, LIS, TRI)) continue;// 子暫存器/lanemask
      // if (aliasConflict(PR, V, LIS, PhysAssigned, TRI)) continue; // 別名/RegUnits

      bool Conflict = false;
      for (Register OV : PhysAssigned[PR]) {
        LiveInterval &OtherLI = LIS.getInterval(OV);
        if (LI.overlaps(OtherLI)) { Conflict = true; break; }
      }
      if (Conflict) continue;

      LocalVRM->assignVirt2Phys(V, PR);
      PhysAssigned[PR].push_back(V);
      Assigned = true;
      break;
    }

    if (!Assigned) {
      errs() << "[SegmentTree] cannot assign vreg " << V.id()
            << " without spilling; aborting this MF\n";
      return false;
    }
  }

  // 到這裡：已經完成「無 spill」的極簡分配
  return true; // 表示我們修改了 MF（VRM 被更新）
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
