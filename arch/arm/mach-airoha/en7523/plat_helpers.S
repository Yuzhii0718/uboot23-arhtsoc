
	.globl	jumparch64

	.extern bl31_base_addr
	.extern rst_vector_base_addr

	.section .text.arch64

jumparch64:
	mov r4, r1   /* r4 argument */
	mov r5, r2   /* r5 argument */
	mov r6, r0   /* keep LK jump addr */
	mov r7, r3   /* r3 = TEE boot entry, relocate to r7 */

	/* setup the reset vector base address after warm reset to Aarch64 */
	ldr r0, =bl31_base_addr
	ldr r0,[r0]

	ldr r1, =rst_vector_base_addr
	ldr r1,[r1]
	str r0,[r1]

	/* setup the excution state after warm reset: 1:Aarch64, 0:Aarch32 */
	mov r2, #0x3
	mcr p15, 0, r2, c12, c0, 2

	isb
	dsb

	mov r0, #0
	mov r1, #0
	mov r2, #0
	mov r3, #0

wfi_loop:
	/* enter WFI to request a warm reset */
	wfi
	b wfi_loop

