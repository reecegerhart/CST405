# ============ Generated MIPS ============
.data
__nl: .asciiz "\n"

.text
.globl main

# ==== function main ====
main:
    addi $sp, $sp, -32        # build activation record
    sw   $ra, 28($sp)        # save return address
    # int x lives at 0($sp)
    # int y lives at 4($sp)
    # int total lives at 8($sp)
    li   $t0, 5            # x = 5
    li   $t1, 10            # y = 10
    li   $t2, 15            # t1 = 15
    li   $t3, 18            # t0 = 18
    li   $t4, 18            # total = 18
    li   $a0, 5            # print 5
    li   $v0, 1              # syscall 1 = print integer
    syscall
    la   $a0, __nl           # then a newline
    li   $v0, 4              # syscall 4 = print string
    syscall
    li   $a0, 10            # print 10
    li   $v0, 1              # syscall 1 = print integer
    syscall
    la   $a0, __nl           # then a newline
    li   $v0, 4              # syscall 4 = print string
    syscall
    li   $a0, 18            # print 18
    li   $v0, 1              # syscall 1 = print integer
    syscall
    la   $a0, __nl           # then a newline
    li   $v0, 4              # syscall 4 = print string
    syscall
    li   $v0, 0            # return 0
    # write back live values (return)
    # spill: x -> memory
    sw   $t0, 0($sp)        # x
    # spill: y -> memory
    sw   $t1, 4($sp)        # y
    # spill: t1 -> memory
    sw   $t2, 12($sp)        # t1
    # spill: t0 -> memory
    sw   $t3, 16($sp)        # t0
    # spill: total -> memory
    sw   $t4, 8($sp)        # total
    j    main__epilogue
main__epilogue:
    lw   $ra, 28($sp)        # restore return address
    addi $sp, $sp, 32        # discard activation record
    li   $v0, 10             # syscall 10 = exit
    syscall
