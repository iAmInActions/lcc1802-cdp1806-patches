;putc.inc contains character output runtime routine for LCC-xr18
;input is in bottom byte of regArg1 (r13 as of the christmas compiler)
_putc:
_out5:
	ld2	regArg1,'D',ARGBUF+0,0 ;fetch char arg from fixed RAM argument area
	glo	regArg1
	dec	sp
	str	sp
	out	5
	Cretn