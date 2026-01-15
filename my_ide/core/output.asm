.data

.text
busydelay:
    addi sp, sp, -24
    sw ra, 20(sp)
    sw s0, 16(sp)
    addi s0, sp, 24
    sw a0, -12(s0)
.lop1:
    lw t0, -12(s0)
    addi t1, x0, 0
    slt t2, t1, t0
    beq t2, x0, .end2
    lw t0, -12(s0)
    addi t1, x0, 1
    sub t2, t0, t1
    sw t2, -12(s0)
    jal x0, .lop1
.end2:
    addi t0, x0, 0
    addi t1, x0, -972
    sw t0, 0(t1)
    addi a0, x0, 0
    jal x0, .L_ret_busydelay_0
.L_ret_busydelay_0:
    lw s0, 16(sp)
    lw ra, 20(sp)
    addi sp, sp, 24
    jalr x0, 0(ra)

playcommon:
    addi sp, sp, -24
    sw ra, 20(sp)
    sw s0, 16(sp)
    addi s0, sp, 24
    sw a0, -12(s0)
    sw a1, -16(s0)
    lw t0, -12(s0)
    addi t1, x0, -976
    sw t0, 0(t1)
    addi t0, x0, 1
    addi t1, x0, -972
    sw t0, 0(t1)
    lw t0, -16(s0)
    addi a0, t0, 0
    jal ra, busydelay
    addi t0, a0, 0
    addi a0, x0, 0
    jal x0, .L_ret_playcommon_3
.L_ret_playcommon_3:
    lw s0, 16(sp)
    lw ra, 20(sp)
    addi sp, sp, 24
    jalr x0, 0(ra)

playnote:
    addi sp, sp, -24
    sw ra, 20(sp)
    sw s0, 16(sp)
    addi s0, sp, 24
    sw a0, -12(s0)
    lw t0, -12(s0)
    addi a0, t0, 0
    lui  t0, 15
    addi t0, t0, -1440
    addi a1, t0, 0
    jal ra, playcommon
    addi t0, a0, 0
    addi a0, x0, 0
    jal x0, .L_ret_playnote_4
.L_ret_playnote_4:
    lw s0, 16(sp)
    lw ra, 20(sp)
    addi sp, sp, 24
    jalr x0, 0(ra)

playlongnote:
    addi sp, sp, -24
    sw ra, 20(sp)
    sw s0, 16(sp)
    addi s0, sp, 24
    sw a0, -12(s0)
    lw t0, -12(s0)
    addi a0, t0, 0
    lui  t0, 29
    addi t0, t0, 1216
    addi a1, t0, 0
    jal ra, playcommon
    addi t0, a0, 0
    addi a0, x0, 0
    jal x0, .L_ret_playlongnote_5
.L_ret_playlongnote_5:
    lw s0, 16(sp)
    lw ra, 20(sp)
    addi sp, sp, 24
    jalr x0, 0(ra)

playshortnote:
    addi sp, sp, -24
    sw ra, 20(sp)
    sw s0, 16(sp)
    addi s0, sp, 24
    sw a0, -12(s0)
    lw t0, -12(s0)
    addi a0, t0, 0
    lui  t0, 7
    addi t0, t0, 1328
    addi a1, t0, 0
    jal ra, playcommon
    addi t0, a0, 0
    addi a0, x0, 0
    jal x0, .L_ret_playshortnote_6
.L_ret_playshortnote_6:
    lw s0, 16(sp)
    lw ra, 20(sp)
    addi sp, sp, 24
    jalr x0, 0(ra)

.globl main
main:
    lui sp, 0x00010
    addi sp, sp, -24
    sw ra, 20(sp)
    sw s0, 16(sp)
    addi s0, sp, 24
    lui  t0, 30577
    addi t0, t0, -274
    addi a0, t0, 0
    jal ra, playnote
    addi t0, a0, 0
    lui  t0, 27233
    addi t0, t0, -691
    addi a0, t0, 0
    jal ra, playnote
    addi t0, a0, 0
    lui  t0, 24225
    addi t0, t0, -1062
    addi a0, t0, 0
    jal ra, playnote
    addi t0, a0, 0
    lui  t0, 30577
    addi t0, t0, -274
    addi a0, t0, 0
    jal ra, playnote
    addi t0, a0, 0
    lui  t0, 30577
    addi t0, t0, -274
    addi a0, t0, 0
    jal ra, playnote
    addi t0, a0, 0
    lui  t0, 27233
    addi t0, t0, -691
    addi a0, t0, 0
    jal ra, playnote
    addi t0, a0, 0
    lui  t0, 24225
    addi t0, t0, -1062
    addi a0, t0, 0
    jal ra, playnote
    addi t0, a0, 0
    lui  t0, 30577
    addi t0, t0, -274
    addi a0, t0, 0
    jal ra, playnote
    addi t0, a0, 0
    lui  t0, 24225
    addi t0, t0, -1062
    addi a0, t0, 0
    jal ra, playnote
    addi t0, a0, 0
    lui  t0, 22913
    addi t0, t0, -1232
    addi a0, t0, 0
    jal ra, playnote
    addi t0, a0, 0
    lui  t0, 20401
    addi t0, t0, -1545
    addi a0, t0, 0
    jal ra, playlongnote
    addi t0, a0, 0
    lui  t0, 24225
    addi t0, t0, -1062
    addi a0, t0, 0
    jal ra, playnote
    addi t0, a0, 0
    lui  t0, 22913
    addi t0, t0, -1232
    addi a0, t0, 0
    jal ra, playnote
    addi t0, a0, 0
    lui  t0, 20401
    addi t0, t0, -1545
    addi a0, t0, 0
    jal ra, playlongnote
    addi t0, a0, 0
    lui  t0, 20401
    addi t0, t0, -1545
    addi a0, t0, 0
    jal ra, playshortnote
    addi t0, a0, 0
    lui  t0, 18193
    addi t0, t0, -1823
    addi a0, t0, 0
    jal ra, playshortnote
    addi t0, a0, 0
    lui  t0, 20401
    addi t0, t0, -1545
    addi a0, t0, 0
    jal ra, playshortnote
    addi t0, a0, 0
    lui  t0, 22913
    addi t0, t0, -1232
    addi a0, t0, 0
    jal ra, playshortnote
    addi t0, a0, 0
    lui  t0, 24225
    addi t0, t0, -1062
    addi a0, t0, 0
    jal ra, playnote
    addi t0, a0, 0
    lui  t0, 30577
    addi t0, t0, -274
    addi a0, t0, 0
    jal ra, playnote
    addi t0, a0, 0
    lui  t0, 20401
    addi t0, t0, -1545
    addi a0, t0, 0
    jal ra, playshortnote
    addi t0, a0, 0
    lui  t0, 18193
    addi t0, t0, -1823
    addi a0, t0, 0
    jal ra, playshortnote
    addi t0, a0, 0
    lui  t0, 20401
    addi t0, t0, -1545
    addi a0, t0, 0
    jal ra, playshortnote
    addi t0, a0, 0
    lui  t0, 22913
    addi t0, t0, -1232
    addi a0, t0, 0
    jal ra, playshortnote
    addi t0, a0, 0
    lui  t0, 24225
    addi t0, t0, -1062
    addi a0, t0, 0
    jal ra, playnote
    addi t0, a0, 0
    lui  t0, 30577
    addi t0, t0, -274
    addi a0, t0, 0
    jal ra, playnote
    addi t0, a0, 0
    lui  t0, 30577
    addi t0, t0, -274
    addi a0, t0, 0
    jal ra, playnote
    addi t0, a0, 0
    lui  t0, 40817
    addi t0, t0, 1006
    addi a0, t0, 0
    jal ra, playnote
    addi t0, a0, 0
    lui  t0, 30577
    addi t0, t0, -274
    addi a0, t0, 0
    jal ra, playlongnote
    addi t0, a0, 0
    lui  t0, 30577
    addi t0, t0, -274
    addi a0, t0, 0
    jal ra, playnote
    addi t0, a0, 0
    lui  t0, 40817
    addi t0, t0, 1006
    addi a0, t0, 0
    jal ra, playnote
    addi t0, a0, 0
    lui  t0, 30577
    addi t0, t0, -274
    addi a0, t0, 0
    jal ra, playlongnote
    addi t0, a0, 0
    addi a0, x0, 0
    jal x0, .L_ret_main_7
.L_ret_main_7:
    lw s0, 16(sp)
    lw ra, 20(sp)
    addi sp, sp, 24
    jalr x0, 0(ra)

