global single

.code 32

single:

	;# Sichern der benoetigten Register
	PUSH {r4-r10, lr}

	;# Laden der restlichen beiden Uebergabewerte vom Stack
	ldr r4, [sp, #32]
	ldr r5, [sp, #36]


	;# r6 ist window

	mov r7, #0

schleifeX:
	
	mov r8, #0

	schleifeY:
	
		add r8, #1
		cmp r8, r3
		blt schleifeY

	add r7, #1
	cmp r7, r4
	blt schleifeX

	;# Stack wiederherstellen
	POP {r4-r10, lr}
	BX lr