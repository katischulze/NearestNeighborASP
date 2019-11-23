.global window
.extern malloc

.code 32
 
window:

	;# Push required registers
	PUSH {r4-r11, lr}

	;# Read remaining parameters from stack
	ldr r4, [sp, #36]
	ldr r5, [sp, #40]
	
	mul r12, r5, r2	;# ogWidth * yPos
	add r12, r12, r1	;# (ogWidth * yPos) + xPos
	mov r11, #3			;# r11 not yet in use
	mul r12, r12, r11	;# ((ogWidth * yPos) + xPos) * 3
	add r0, r0, r12	;# we now no longer need xPos, yPos -> more registers
	
	;# Malloc for new Image
	push {r0-r3}
	
	;# Size: width * height * 3 (bytes)
	mul r0, r3, r4
	mul r0, r0, r11	;# 3 from before
	mov r11, r0			;# save size in r11
		
	bl malloc
	mov r10, r0		;# move new pointer to r10
	pop {r0-r3}
	
	;# last pointer of new array
	add r11, r11, r10
	
	;# LoopY
	mov r1, r10			;# counter new image
	mov r2, r0			;# counter old image
	mov r12, #3
	mul r3, r3, r12	;# multiply width by 3 (Is now per byte, not pixel)
	mov r7, r10			;# r7 = new image pointer (Will be modified in the loop
	mul r4, r5, r12 	;# multiply width by #3 into r4 (Don't need height anymore)
	loopY:
		
		;# LoopX
		mov r9, #0		;# reset counter
		mov r2, r0
		add r0, r0, r4	;# move to next line
		mov r1, r7		
		add r7, r7, r3	;# move to next line
		loopX:
			
			add r1, r1, #24
			cmp r1, r11
			sub r1, r1, #24
			bge SISD			;# if image doesn't have at least 8 pixels left to be written,
								;# jump to SISD. Avoid Segmentation faults.
			
			SIMD:
			;# Copy colors and increment
			;# Copies 8 pixels at once
			vld3.8 {d8, d9, d10}, [r2]!
			vst3.8 {d8, d9, d10}, [r1]!
			b continue
			
			;# Fill rest with SISD
			SISD:
			
				;#Load colors per byte to new image 
				ldrb r6, [r2], #1
				strb r6, [r1], #1
				
				ldrb r6, [r2], #1
				strb r6, [r1], #1
				
				ldrb r6, [r2], #1
				strb r6, [r1], #1
				
				cmp r1, r11
				blt SISD
			b continue

			continue:

			;# loopX
			add r9, r9, #24		;# increment by 24, for 8 pixels + 3 bytes
			cmp r9, r3
			blt loopX

		;# loopY
		cmp r1, r11			;# r1 has already been incremented
		blt loopY

	mov r0, r10 ;# return new image

	;# Restore stack
	POP {r4-r11, lr}
	BX lr
