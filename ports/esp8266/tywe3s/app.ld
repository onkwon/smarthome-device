INCLUDE ports/esp8266/tywe3s/common.ld

MEMORY
{
	rom (rx)	: ORIGIN = 0x402C0000, LENGTH = 128K
}

PROVIDE(__rom_start = ORIGIN(rom));
PROVIDE(__rom_size  = LENGTH(rom));

SECTIONS
{
	.text __rom_start :
	{
		KEEP(*(.text._app_init))
		*(.text*)

		. = ALIGN(4);
		*(.rodata*)

		. = ALIGN(4);
		PROVIDE(__app_text_end = .);
	} > rom

	.data :
	{
		. = ALIGN(4);
		PROVIDE(__app_data_start = .);

		*(.data*)

		. = ALIGN(4);
		PROVIDE(__app_data_end = .);
	} > ram AT > rom

	.bss (NOLOAD) :
	{
		. = ALIGN(4);
		PROVIDE(__app_bss_start = .);

		*(.bss*)
		*(COMMON)

		. = ALIGN(4);
		PROVIDE(__app_bss_end = .);
	} > ram AT > rom

	.noinit (NOLOAD) :
	{
		PROVIDE(__noinit_data_start = .);
		KEEP(*(.noinit))
		PROVIDE(__noinit_data_end = .);
	} > ram_noinit
}
