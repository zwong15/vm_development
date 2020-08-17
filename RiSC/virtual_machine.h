#ifndef VIRTUAL_MACHINE_H
#define VIRTUAL_MACHINE_H

#include <stdbool.h>
#include <stdint.h>

typedef struct  RiSC_VM RiSC_VM;

RiSC_VM*  vm_init (char filename[]);
void vm_shutdown  (RiSC_VM* vm);
void vm_fetch (RiSC_VM* vm);
void vm_decode (RiSC_VM* vm);
void vm_execute (RiSC_VM* vm);
bool vm_running (RiSC_VM* vm);
void vm_print_regs (RiSC_VM* vm);
void vm_print_data (RiSC_VM* vm);

#endif
