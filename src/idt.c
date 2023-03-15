#include "lib-header/stdtype.h"
#include "lib-header/idt.h"

/**
 * interupt_descriptor_table, predefined IDT.
 * Initial IDTGate already set properly according to IDT definition in Intel Manual & OSDev.
 * Table entry : [...].
 */
struct InteruptDescriptorTable interupt_descriptor_table = {
    .table = {
    }
};

/**
 * _idt_gdtr, predefined system IDTR. 
 * IDT pointed by this variable is already set to point interupt_descriptor_table above.
 * From: IDTR.size is IDT size minus 1.
 */
struct IDTR _idt_gdtr = {
    .size = sizeof(interupt_descriptor_table) - 1,
    .address = &interupt_descriptor_table
};