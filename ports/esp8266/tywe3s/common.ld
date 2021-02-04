MEMORY
{
	ram_noinit(rwx) : ORIGIN = 0x3FFE8000, LENGTH = 0x8
	ram (rwx)	: ORIGIN = ORIGIN(ram_noinit) + LENGTH(ram_noinit),
				LENGTH = 3K - LENGTH(ram_noinit)
	bootopt (rx)	: ORIGIN = 0x402B0000, LENGTH = 4K
	dfu (rx)	: ORIGIN = 0x402D1000, LENGTH = 128K
}

PROVIDE(__bootopt = ORIGIN(bootopt) & 0xFFFFF);
PROVIDE(__bootopt_size = LENGTH(bootopt));

PROVIDE(__dfu_partition = ORIGIN(dfu) & 0xFFFFF);

PROVIDE(__ram_start = ORIGIN(ram));
PROVIDE(__ram_size  = LENGTH(ram));
PROVIDE(__ram_end   = ORIGIN(ram) + LENGTH(ram));
