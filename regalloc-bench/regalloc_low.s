	.build_version macos, 15, 0	sdk_version 15, 5
	.section	__TEXT,__text,regular,pure_instructions
	.globl	_regalloc_low                   ; -- Begin function regalloc_low
	.p2align	2
_regalloc_low:                          ; @regalloc_low
	.cfi_startproc
; %bb.0:
	subs	w9, w0, #1
	b.lt	LBB0_2
; %bb.1:
	sub	w8, w0, #2
	umull	x8, w9, w8
	lsr	x8, x8, #1
	add	w8, w8, w0, lsl #1
	sub	w0, w8, #1
	ret
LBB0_2:
	mov	w0, #0                          ; =0x0
	ret
	.cfi_endproc
                                        ; -- End function
.subsections_via_symbols
