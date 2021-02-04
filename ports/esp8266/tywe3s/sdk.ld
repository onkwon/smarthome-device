MEMORY
{
	iram0_0_seg (RX) : org = 0x40100000, len = 0xC000
	/* rear 150K, starting from 0x402A6010, allocated for App */
	iram0_2_seg (RX) : org = 0x40210010, len = 0xAD000 /* 708,608 bytes */
	/* front 4K, starting from 0x3FFE8000, allocated for App */
	dram0_0_seg (RW) : org = 0x3FFE8C00, len = 0x13400 /* 78,848 bytes */
	rtc_data_seg(RW) : org = 0x60001200, len = 0x200
}
