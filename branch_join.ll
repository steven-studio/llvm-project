; ModuleID = 'branch_join'
declare void @use(i32)

define void @bj(i32* %p) {
entry:
  %c = icmp eq i32 0, 0
  br i1 %c, label %T, label %F
T:
  store i32 1, i32* %p
  br label %J
F:
  store i32 2, i32* %p
  br label %J
J:
  %x = load i32, i32* %p
  call void @use(i32 %x)
  ret void
}

