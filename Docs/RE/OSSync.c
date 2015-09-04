
__OSSystemCallVectorStart:
	mfspr     r9, hid0
	ori       r10, r9, 8
	mtspr     hid0, r10
	isync
	sync
	mtspr     hid0, r9
	rfi
__OSSystemCallVectorEnd:

__OSInitSystemCall ()
{
	Addr = OSPhysicalToCached ( 0xC00 );

	memcpy ( Addr,
		 	 __OSSystemCallVectorStart,
		 	 __OSSystemCallVectorEnd - __OSSystemCallVectorStart );

	DCFlushRangeNoSync ( Addr, 0x100 );

	sync;

	ICInvalidateRange ( Addr, 0x100 );
}