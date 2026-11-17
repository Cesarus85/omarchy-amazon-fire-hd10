return_site = 0x2df74; original_fault = 0xb48;
ENTRY(proxy_hook)
SECTIONS { . = 0x50000; .text : { *(.text.entry) *(.text*) *(.rodata*) } /DISCARD/ : { *(.ARM.exidx*) *(.ARM.attributes) *(.comment) } }
