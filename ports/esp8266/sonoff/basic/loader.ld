INCLUDE ports/esp8266/sonoff/basic/common.ld

MEMORY
{
	rom (rx)	: ORIGIN = 0x402BE000, LENGTH = 4K
	app (rx)	: ORIGIN = 0x402C0000, LENGTH = 128K
}

PROVIDE(__app_partition = ORIGIN(app) & 0xFFFFF);

SECTIONS
{
	.text :
	{
		KEEP(*(.text.apploader))
		*(.text*)

		. = ALIGN(4);
		*(.rodata*)

		. = ALIGN(4);
		PROVIDE(__loader_text_end = .);
	} > rom

	.data :
	{
		. = ALIGN(4);
		PROVIDE(__loader_data_start = .);

		*(.data*)

		. = ALIGN(4);
		PROVIDE(__loader_data_end = .);
	} > ram AT > rom

	.bss (NOLOAD) :
	{
		. = ALIGN(4);
		PROVIDE(__loader_bss_start = .);

		*(.bss*)
		*(COMMON)

		. = ALIGN(4);
		PROVIDE(__loader_bss_end = .);
	} > ram AT > rom

	.noinit (NOLOAD) :
	{
		PROVIDE(__noinit_data_start = .);
		KEEP(*(.noinit))
		PROVIDE(__noinit_data_end = .);
	} > ram_noinit
}
