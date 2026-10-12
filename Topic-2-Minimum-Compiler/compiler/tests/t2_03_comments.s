# ============ Generated MIPS ============
.data
__nl: .asciiz "\n"

.text
.globl main

# ==== function main ====
main:
    addi $sp, $sp, -16        # build activation record
    sw   $ra, 12($sp)        # save return address
    # int answer lives at 0($sp)
    li   $t0, 42            # t0 = 42
    li   $t1, 42            # answer = 42
    li   $a0, 42            # print 42
    li   $v0, 1              # syscall 1 = print integer
    syscall
    la   $a0, __nl           # then a newline
    li   $v0, 4              # syscall 4 = print string
    syscall
    li   $v0, 0            # return 0
    # write back live values (return)
    # spill: t0 -> memory
    sw   $t0, 4($sp)        # t0
    # spill: answer -> memory
    sw   $t1, 0($sp)        # answer
    j    main__epilogue
main__epilogue:
    lw   $ra, 12($sp)        # restore return address
    addi $sp, $sp, 16        # discard activation record
    li   $v0, 10             # syscall 10 = exit
    syscall
