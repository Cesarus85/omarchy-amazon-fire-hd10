/* SPDX-License-Identifier: MIT */
.syntax unified
.cpu cortex-m4
.thumb
.section .text.entry,"ax",%progbits
.global proxy_hook
.thumb_func
proxy_hook:
 push.w {r0-r12,lr}
 mrs r0, apsr
 push {r0,r1}
 /* Grant this private scratch page RW access in an unused MPU slot. */
 ldr r0,=0xe000ed98
 movs r1,#6
 str r1,[r0]
 ldr r2,[r0,#8]
 tst r2,#1
 bne skip_init
 ldr r1,=0x57000
 str r1,[r0,#4]
 ldr r1,=0x130c000f
 str r1,[r0,#8]
 dsb
 isb
 bl proxy_init
skip_init:
 pop {r0,r1}
 msr APSR_nzcvq,r0
 pop.w {r0-r12,lr}
 mov.w r3,#0xa6000000
 b.w return_site
.global proxy_exception
.thumb_func
proxy_exception:
 tst lr,#4
 ite eq
 mrseq r0,msp
 mrsne r0,psp
 tst lr,#0x10
 it eq
 addeq r0,r0,#72
 push.w {r4-r11,lr}
 sub sp,sp,#4
 add r1,sp,#4
 bl proxy_fault
 cmp r0,#0
 add sp,sp,#4
 pop.w {r4-r11,lr}
 beq failed
 bx lr
failed:
 b.w original_fault
