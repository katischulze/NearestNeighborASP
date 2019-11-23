.global zoomSISD
.extern malloc

.code 32


zoomSISD:

	;# Push required registers
	PUSH {r4-r11, lr}

	;# Change Position of Input data
	mov r4, r3
	mov r3, r2
	mov r2, r1

	;# Malloc for new Image
	mov r10, r0		;# backup old image in r10
	
	;# Size: width * height * (scale*scale) * 3 (bytes)
	mul r0, r2, r3
	mul r0, r0, r4
	mul r0, r0, r4
	mov r12, #3	
	mul r0, r0, r12
	
	push {r1-r3}	
	bl malloc
	pop {r1-r3}
	mov r1, r0		;# move new pointer to r1
	
	mov r0, r10		;# undo backup

	mul r11, r2, r3
	mov r12, #3
	mul r11, r11, r12	;# Size of old pic in bytes
	add r11, r11, r0	;# to pointer
	
	mov r12, #3
	mul r7, r4, r12	;# r7 is our increment between pixels in scaled image
	mul r12, r7, r2
	sub r5, r4, #1
	mul r12, r12, r5	;# r12 is the increment between lines in scaled image
	
	mov r6, r1			;# Copy pointers
	mov r5, r0			;# Loop Counter
	mov r8, #0			;# Width counter
	loop:
	
		;# Copy pixel over
		;# Also increment r5
		ldrb r9, [r5], #1
		strb r9, [r6]
		
		ldrb r9, [r5], #1
		strb r9, [r6, #1]
		
		ldrb r9, [r5], #1
		strb r9, [r6, #2]
		
		;# loop
		add r6, r6, r7		;# increment new pointer
		add r8, r8, #1
		cmp r8, r2			;# if Width counter >= width
		addge r6, r6, r12	;# Add line increment to pointer in old picture
		movge r8, #0		;# Reset counter
		cmp r5, r11			
		blt loop			;# Loop through entire image
	
	;# FILL 
	mul r2, r2, r4	;# Scale width with scale-factor
	mul r3, r3, r4	;# Scale height with scale-factor
	
	mov r5, #0		;# Loop Counter X
	fillLoopX:

		mov r6, #0		;# Loop Counter  Y
		fillLoopY:
		
			;# The parent is the pixel, that currently has the color we want on the current pixel
			;# parentX coord
			add r12, r5, r4, LSR #1
			udiv r7, r12, r4
			mul r7, r7, r4
			;# parentY coord
			add r12, r6, r4, LSR #1
			udiv r8, r12, r4
			mul r8, r8, r4
			
			;# check out of bounds
			cmp r7, r2 
			subeq r7, r7, r4	;# if out of bounds subtract factor from x coord
			cmp r8, r3
			subeq r8, r8, r4	;# if out of bounds subtract factor from y coord
			
			;# map pixel onto array (x + y * width) * 3
			mov r12, #3
			mul r9, r6, r2
			add r9, r9, r5
			mul r9, r9, r12
			
			;# map parent onto array (x + y * width) * 3
			mul r11, r8, r2
			add r11, r11, r7
			mul r11, r11, r12
			
			;# Load & Store Colors
			ldrb r10, [r1, r11]
			strb r10, [r1, r9]

			add r11, r11, #1
			add r9, r9, #1
			ldrb r10, [r1, r11]
			strb r10, [r1, r9]
			
			add r11, r11, #1
			add r9, r9, #1
			ldrb r10, [r1, r11]
			strb r10, [r1, r9]
			
			;# fillLoopY
			add r6, r6, #1
			cmp r6, r3
			blt fillLoopY

		;# fillLoopX
		add r5, r5, #1
		cmp r5, r2
		blt fillLoopX
	
	mov r0, r1	;# return pointer to new image
	
	;# Restore registers
	POP {r4-r11, lr}
	BX lr
