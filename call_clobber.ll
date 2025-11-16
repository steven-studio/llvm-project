; ModuleID = 'call_clobber'

; 外部宣告：一個是可能修改 (mayclobber)，一個是唯讀 (readonly)
declare void @foo_mayclobber(i8*)
declare void @foo_readonly(i8*) readonly

define void @call_mayclobber(i32* %p) {
entry:
  store i32 1, i32* %p
  %p8 = bitcast i32* %p to i8*
  call void @foo_mayclobber(i8* %p8)
  %x = load i32, i32* %p
  ret void
}

define void @call_readonly(i32* %p) {
entry:
  store i32 2, i32* %p
  %p8 = bitcast i32* %p to i8*
  call void @foo_readonly(i8* %p8)
  %x = load i32, i32* %p
  ret void
}

define void @call_chain(i32* %p) {
entry:
  store i32 1, i32* %p
  %p8 = bitcast i32* %p to i8*
  call void @foo_mayclobber(i8* %p8)
  %a = load i32, i32* %p

  store i32 %a, i32* %p
  call void @foo_mayclobber(i8* %p8)
  %b = load i32, i32* %p

  store i32 %b, i32* %p
  call void @foo_readonly(i8* %p8)
  %c = load i32, i32* %p

  ret void
}
