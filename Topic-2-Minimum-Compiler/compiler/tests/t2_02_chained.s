# ============ Generated MIPS ============
.data
__nl: .asciiz "\n"

.text
.globl main

# ==== function main ====
main:
    addi $sp, $sp, -40        # build activation record
    sw   $ra, 36($sp)        # save return address
    # int a lives at 0($sp)
    # int b lives at 4($sp)
    # int c lives at 8($sp)
    # int r lives at 12($sp)
    li   $t0, 1            # a = 1
    li   $t1, 2            # b = 2
    li   $t2, 3            # c = 3
    li   $t3, 3            # t1 = 3
    li   $t4, 6            # t0 = 6
    li   $t5, 6            # r = 6
    li   $a0, 6            # print 6
    li   $v0, 1              # syscall 1 = print integer
    syscall
    la   $a0, __nl           # then a newline
    li   $v0, 4              # syscall 4 = print string
    syscall
    li   $t6, 12            # t2 = 12
    li   $t3, 18            # t1 = 18
    li   $t4, 21            # t0 = 21
    li   $t5, 21            # r = 21
    li   $a0, 21            # print 21
    li   $v0, 1              # syscall 1 = print integer
    syscall
    la   $a0, __nl           # then a newline
    li   $v0, 4              # syscall 4 = print string
    syscall
    li   $v0, 0            # return 0
    # write back live values (return)
    # spill: a -> memory
    sw   $t0, 0($sp)        # a
    # spill: b -> memory
    sw   $t1, 4($sp)        # b
    # spill: c -> memory
    sw   $t2, 8($sp)        # c
    # spill: t1 -> memory
    sw   $t3, 16($sp)        # t1
    # spill: t0 -> memory
    sw   $t4, 20($sp)        # t0
    # spill: r -> memory
    sw   $t5, 12($sp)        # r
    # spill: t2 -> memory
    sw   $t6, 24($sp)        # t2
    j    main__epilogue
main__epilogue:
    lw   $ra, 36($sp)        # restore return address
    addi $sp, $sp, 40        # discard activation record
    li   $v0, 10             # syscall 10 = exit
    syscall
