//-------------------------------------------------------------------------------------------------
// "Origin of the Bucket"
// Routine: 256 bytes vector creature with bucket
// Code: Cruzer/Camelot 2006-07
// Asm: KickAss 2.23
//-------------------------------------------------------------------------------------------------

//Memory...

.var xLo =	$02
.var yLo =	$03
.var xHi =	$04
.var yHi =	$05
.var xAddLo =	$06
.var yAddLo =	$07
.var xAddHi =	$08
.var yAddHi =	$09
.var p0 =	$0a
.var x0 =	$10
.var y0 =	$11
.var x1 =	$12
.var y1 =	$13
.var base =	$0801
.var charset =	$2000
.var screen =	$3c00

.pc = base "code"

//Basic...
		.byte $0b,$08,$39,$05,$9e,$32,$31,$36,$31
iter:		.byte $00
		.byte $00,$00

coordsX:

.by	123,96,74,52,49,46,42,39,46,38,40,37,32,24,22,23,16,23,18,24,40,50,25,24
.by	27,25,20,8,18,40,46,43,11,15,31,41,34,42,36,45,42,43,13,15,1,1,16,37,16,123

.var numCoords = * - coordsX
.print "numCoords:" + numCoords


coordsY:

.by	126,54,22,11,6,5,7,6,2,6,1,5,4,6,13,20,24,22,27,23,16,22,29,23,41,55,70
.by	74,80,76,67,66,72,84,81,83,89,92,100,98,66,68,74,85,98,127,127,101,127,127


// clear charset and screen...

!:
sc:	sta charset-$100,x
	inx
	bne !-
	txa
	inc sc+2
	bpl !-

	lda #$f8
	sta $d018

//setup charset...

	ldy #$0f
!loop:
	clc
	tya
	ldx #0
ss:	sta screen+$032c,x
	inx
	adc #$10
	bcc ss
	lda ss+1
	sbc #40
	sta ss+1
	bcs !+
	dec ss+2
!:	dey
	bpl !loop-

mainLoop:

cnt:	ldx #numCoords - 2

	lda coordsX+0,x
	sta xHi
	lda coordsY+0,x
	sta yHi
	lda coordsX+1,x
stop:	beq stop
	pha
	lda coordsY+1,x
	pha

	dec cnt+1
drawLine:
	ldx #1
!:
	pla
	sec
	sbc xHi,x
	sta xAddLo,x
	lda #$ff
	adc #$00
	sta xAddHi,x
	dex
	bpl !-

iterLoop:
	ldx #2
!:
	lda xLo-1,x
	clc
	adc xAddLo-1,x
	sta xLo-1,x
	lda xHi-1,x
	adc xAddHi-1,x
	sta xHi-1,x
	dex
	bne !-

	lsr
	sec
	ror
	lsr
	lsr
	sta p0+1
	txa
	ror
	sta p0+0
	lda xHi
	and #$07
	tay
	txa
	sec
!:	ror
	dey
	bpl !-
	ldy yHi
	ora (p0),y
	sta (p0),y

	inc iter
	bne iterLoop

	beq mainLoop

