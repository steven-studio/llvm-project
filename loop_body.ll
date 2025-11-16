; ModuleID = 'alias_ish'

declare void @sink(i32)

define void @mix(i32* %p, i32* %q, i32 %n) {
entry:
  br label %loop

loop:
  %i = phi i32 [ 0, %entry ], [ %i.next, %loop ]

  store i32 %i, i32* %p
  %a = load i32, i32* %q

  store i32 %a, i32* %q
  %b = load i32, i32* %p

  store i32 %b, i32* %p
  %c = load i32, i32* %q

  %sum = add i32 %a, %c
  call void @sink(i32 %sum)

  %i.next = add i32 %i, 1
  %cond = icmp slt i32 %i.next, %n
  br i1 %cond, label %loop, label %exit

exit:
  ret void
}

; define void @mix_noalias(i32* noalias %p, i32* noalias %q, i32 %n) { ... }
