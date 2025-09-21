; ModuleID = 'test'
source_filename = "test"

define i32 @foo(i32* %p, i32* %q) {
entry:
  store i32 42, i32* %p
  %x = load i32, i32* %p
  %y = load i32, i32* %p
  %z = add i32 %x, %y
  store i32 %z, i32* %q
  ret i32 %z
}
