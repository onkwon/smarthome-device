MEMORY
{
	ram (rwx)	: ORIGIN = 0x3FFE8000, LENGTH = 4K
	rom (rx)	: ORIGIN = 0x402A6010, LENGTH = 150K
}

PROVIDE(_rom_start = ORIGIN(rom));
PROVIDE(_rom_size  = ORIGIN(rom) + LENGTH(rom));
PROVIDE(_ram_start = ORIGIN(ram));
PROVIDE(_ram_size  = LENGTH(ram));
PROVIDE(_ram_end   = ORIGIN(ram) + LENGTH(ram));

SECTIONS
{
	.text _rom_start :
	{
		KEEP(*(.text._app_main))
		*(.text*)

		. = ALIGN(4);
		*(.rodata*)

		. = ALIGN(4);
		PROVIDE(_app_text_end = .);
	} > rom

	.data :
	{
		. = ALIGN(4);
		PROVIDE(_app_data = .);

		*(.data*)

		. = ALIGN(4);
		PROVIDE(_app_data_end = .);
	} > ram AT > rom

	.bss (NOLOAD) :
	{
		. = ALIGN(4);
		PROVIDE(_app_bss = .);

		*(.bss*)
		*(COMMON)

		. = ALIGN(4);
		PROVIDE(_app_bss_end = .);
	} > ram AT > rom
}
