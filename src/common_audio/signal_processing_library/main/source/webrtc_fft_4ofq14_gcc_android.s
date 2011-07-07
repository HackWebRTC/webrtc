  .globl FFT_4OFQ14

FFT_4OFQ14:
  stmdb       sp!, {r4 - r11, lr}
  ldr         lr, =s_Q14S_8
  ldr         lr, [lr]
  cmp         r2, lr
  movgt       r0, #1
  ldmgtia     sp!, {r4 - r11, pc}
  stmdb       sp!, {r1, r2}
  mov         r3, #0
  mov         r2, r2

LBL1:
  add         r12, r0, r3, lsl #2
  add         r12, r12, r2, lsr #1
  ldrsh       r5, [r12, #2]
  ldrsh       r4, [r12], +r2
  ldrsh       r9, [r12, #2]
  ldrsh       r8, [r12], +r2
  ldrsh       r7, [r12, #2]
  ldrsh       r6, [r12], +r2
  ldrsh       r11, [r12, #2]
  ldrsh       r10, [r12], +r2
  add         r4, r4, r6
  add         r5, r5, r7
  sub         r6, r4, r6, lsl #1
  sub         r7, r5, r7, lsl #1
  sub         r12, r8, r10
  sub         lr, r9, r11
  add         r10, r8, r10
  add         r11, r9, r11
  sub         r9, r4, r10
  sub         r8, r5, r11
  add         r4, r4, r10
  add         r5, r5, r11
  sub         r10, r6, lr
  add         r11, r7, r12
  add         r6, r6, lr
  sub         r7, r7, r12
  ldr         lr, =t_Q14R_rad8
  ldrsh       lr, [lr]
  stmdb       sp!, {r2}
  add         r12, r6, r7
  mul         r6, r12, lr
  rsb         r12, r12, r7, lsl #1
  mul         r7, r12, lr
  sub         r12, r11, r10
  mul         r10, r12, lr
  sub         r12, r12, r11, lsl #1
  mul         r11, r12, lr
  ldmia       sp!, {r2}
  stmdb       sp!, {r4 - r11}
  add         r4, r0, r3, lsl #2
  ldrsh       r7, [r4, #2]
  ldrsh       r6, [r4], +r2
  ldrsh       r11, [r4, #2]
  ldrsh       r10, [r4], +r2
  ldrsh       r9, [r4, #2]
  ldrsh       r8, [r4], +r2
  ldrsh       lr, [r4, #2]
  ldrsh       r12, [r4], +r2
  mov         r7, r7, asr #3
  mov         r6, r6, asr #3
  add         r6, r6, r8, asr #3
  add         r7, r7, r9, asr #3
  sub         r8, r6, r8, asr #2
  sub         r9, r7, r9, asr #2
  sub         r4, r10, r12
  sub         r5, r11, lr
  add         r10, r10, r12
  add         r11, r11, lr
  add         r6, r6, r10, asr #3
  add         r7, r7, r11, asr #3
  sub         r10, r6, r10, asr #2
  sub         r11, r7, r11, asr #2
  sub         r12, r8, r5, asr #3
  add         lr, r9, r4, asr #3
  add         r8, r8, r5, asr #3
  sub         r9, r9, r4, asr #3
  ldmia       sp!, {r4, r5}
  add         r6, r6, r4, asr #3
  add         r7, r7, r5, asr #3
  sub         r4, r6, r4, asr #2
  sub         r5, r7, r5, asr #2
  strh        r7, [r1, #2]
  strh        r6, [r1], #4
  ldmia       sp!, {r6, r7}
  add         r8, r8, r6, asr #17
  add         r9, r9, r7, asr #17
  sub         r6, r8, r6, asr #16
  sub         r7, r9, r7, asr #16
  strh        r9, [r1, #2]
  strh        r8, [r1], #4
  ldmia       sp!, {r8, r9}
  add         r10, r10, r8, asr #3
  sub         r11, r11, r9, asr #3
  sub         r8, r10, r8, asr #2
  add         r9, r11, r9, asr #2
  strh        r11, [r1, #2]
  strh        r10, [r1], #4
  ldmia       sp!, {r10, r11}
  add         r12, r12, r10, asr #17
  add         lr, lr, r11, asr #17
  sub         r10, r12, r10, asr #16
  sub         r11, lr, r11, asr #16
  strh        lr, [r1, #2]
  strh        r12, [r1], #4
  strh        r5, [r1, #2]
  strh        r4, [r1], #4
  strh        r7, [r1, #2]
  strh        r6, [r1], #4
  strh        r9, [r1, #2]
  strh        r8, [r1], #4
  strh        r11, [r1, #2]
  strh        r10, [r1], #4
  eor         r3, r3, r2, lsr #4
  tst         r3, r2, lsr #4
  bne         LBL1

  eor         r3, r3, r2, lsr #5
  tst         r3, r2, lsr #5
  bne         LBL1

  mov         r12, r2, lsr #6

LBL2:
  eor         r3, r3, r12
  tst         r3, r12
  bne         LBL1

  movs        r12, r12, lsr #1
  bne         LBL2

  ldmia       sp!, {r1, r2}
  mov         r3, r2, lsr #3
  mov         r2, #0x20
  ldr         r0, =t_Q14S_8
  cmp         r3, #1
  beq         LBL3

LBL6:
  mov         r3, r3, lsr #2
  stmdb       sp!, {r1, r3}
  add         r12, r2, r2, lsl #1
  add         r1, r1, r12
  sub         r3, r3, #1, 16

LBL5:
  add         r3, r3, r2, lsl #14

LBL4:
  ldrsh       r6, [r0], #2
  ldrsh       r7, [r0], #2
  ldrsh       r8, [r0], #2
  ldrsh       r9, [r0], #2
  ldrsh       r10, [r0], #2
  ldrsh       r11, [r0], #2
  ldrsh       r5, [r1, #2]
  ldrsh       r4, [r1], -r2
  sub         lr, r5, r4
  mul         r12, lr, r11
  add         lr, r10, r11, lsl #1
  mla         r11, r5, r10, r12
  mla         r10, r4, lr, r12
  ldrsh       r5, [r1, #2]
  ldrsh       r4, [r1], -r2
  sub         lr, r5, r4
  mul         r12, lr, r9
  add         lr, r8, r9, lsl #1
  mla         r9, r5, r8, r12
  mla         r8, r4, lr, r12
  ldrsh       r5, [r1, #2]
  ldrsh       r4, [r1], -r2
  sub         lr, r5, r4
  mul         r12, lr, r7
  add         lr, r6, r7, lsl #1
  mla         r7, r5, r6, r12
  mla         r6, r4, lr, r12
  ldrsh       r5, [r1, #2]
  ldrsh       r4, [r1]
  mov         r5, r5, asr #2
  mov         r4, r4, asr #2
  add         r12, r4, r6, asr #16
  add         lr, r5, r7, asr #16
  sub         r4, r4, r6, asr #16
  sub         r5, r5, r7, asr #16
  add         r6, r8, r10
  add         r7, r9, r11
  sub         r8, r8, r10
  sub         r9, r9, r11
  add         r10, r12, r6, asr #16
  add         r11, lr, r7, asr #16
  strh        r11, [r1, #2]
  strh        r10, [r1], +r2
  add         r10, r4, r9, asr #16
  sub         r11, r5, r8, asr #16
  strh        r11, [r1, #2]
  strh        r10, [r1], +r2
  sub         r10, r12, r6, asr #16
  sub         r11, lr, r7, asr #16
  strh        r11, [r1, #2]
  strh        r10, [r1], +r2
  sub         r10, r4, r9, asr #16
  add         r11, r5, r8, asr #16
  strh        r11, [r1, #2]
  strh        r10, [r1], #4
  subs        r3, r3, #1, 16
  bge         LBL4
  add         r12, r2, r2, lsl #1
  add         r1, r1, r12
  sub         r0, r0, r12
  sub         r3, r3, #1
  movs        lr, r3, lsl #16
  bne         LBL5
  add         r0, r0, r12
  ldmia       sp!, {r1, r3}
  mov         r2, r2, lsl #2
  cmp         r3, #2
  bgt         LBL6

LBL3:
  mov         r0, #0
  ldmia       sp!, {r4 - r11, pc}
  andeq       r3, r1, r0, lsr #32
  andeq       r10, r1, r12, ror #31
  andeq       r3, r1, r8, lsr #32
