.data

.text
.globl main
main:
    lui sp, 0x00010
    addi sp, sp, -24
    sw ra, 20(sp)
    sw s0, 16(sp)
    addi s0, sp, 24
    addi t0, x0, 1
    sw t0, -12(s0)
.lop1:
    addi t0, x0, 1
    beq t0, x0, .end2
    lui  t0, 732
    addi t0, t0, 1728
    sw t0, -16(s0)
    lw t0, -12(s0)
    addi t1, x0, -928
    sw t0, 0(t1)
    lw t0, -12(s0)
    addi t1, x0, 1
    sll t2, t0, t1
    sw t2, -12(s0)
    lw t0, -12(s0)
    lui  t1, 16
    addi t1, t1, 0
    sub t2, t0, t1
    sltiu t2, t2, 1
    beq t2, x0, .L_end_if4
    addi t0, x0, 1
    sw t0, -12(s0)
.L_end_if4:
.lop5:
    lw t0, -16(s0)
    addi t1, x0, 0
    slt t2, t1, t0
    beq t2, x0, .end6
    lw t0, -16(s0)
    addi t1, x0, 1
    sub t2, t0, t1
    sw t2, -16(s0)
    jal x0, .lop5
.end6:
    jal x0, .lop1
.end2:
    addi a0, x0, 0
    jal x0, .L_ret_main_0
.L_ret_main_0:
    lw s0, 16(sp)
    lw ra, 20(sp)
    addi sp, sp, 24
    jalr x0, 0(ra)

