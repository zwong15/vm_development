#include "virtual_machine.h"
#include "defines.h"
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NUM_REGISTERS 8
#define WORD_SIZE 16
#define MEMORY_SIZE 0xffff
#define STACK_BOTTOM  (MEMORY_SIZE)

#define ADD	0x000
#define ADDI	0x001
#define NAND	0x002
#define LUI	0x003
#define SW	0x004
#define LW	0x005
#define BEQ	0x006
#define JALR	0x007

#define MASK_OPCODE	0xe000
#define MASK_REG_A	0x1c00
#define MASK_REG_B	0x0380
#define MASK_REG_C	0x0007
#define MASK_SIMM	0x007f
#define MASK_UIMM	0x03ff

extern bool print_output;
static char* decimal_to_binary(char* bin, int dec, int nbr_bits);
static void  sign_n_bits(uint16_t* s, unsigned int n);
static uint16_t load_from_file(uint16_t array[], FILE* file);

typedef struct data_t data_t;
typedef struct instruction_t  instruction_t;

struct data_t
{
  uint16_t  data_size;
  uint16_t  data_start;
  uint16_t  text_header;
  uint16_t  text_size;
  uint16_t  text_start;
};

struct instruction_t
{
  uint16_t  opcode;
  uint16_t  reg0;
  uint16_t  reg1;
  uint16_t  reg2;
  uint16_t  simm;
  uint16_t  uimm;
};

struct RiSC_VM
{
  uint16_t  regs[NUM_REGISTERS];
  uint16_t  program[MEMORY_SIZE];
  uint16_t  pc;
  data_t  data;
  instruction_t current_instruction;
  bool  running;
};

static uint16_t load_from_file(uint16_t array[], FILE* file)
{
  uint16_t num_lines = 0;
  char buffer[WORD_SIZE + 1 + 1];
  while(fgets(buffer, sizeof buffer, file))
  {
    strtok(buffer, "\n");
    array[num_lines++] = (uint16_t)strtol(buffer, NULL, 16);
  }
  if(print_output)
  {
    printf("done.\nPrinting loaded addresses and values:\n");
		printf("-------------\n");
		printf("    Address    Value\n");
		for (int i = 0; i < num_lines; ++i)
    {
			printf("    %6d:    0x%04x", i, array[i]);

			printf("%s\n",	i == 0		  ? "  <-- Data header":
					i == array[0] + 1 ? "  <-- Text header":
					"");
		}
		printf("-------------\n");
  }
  return num_lines;
}

static char* decimal_to_binary(char* bin, int dec, int nbr_bits)
{
  int i;
  bin[nbr_bits] = '\0';
  for(i = nbr_bits - 1; i >= 0; --i, dec >>= 1)
  {
    bin[i] = (dec & 1) + '0';
  }
  return bin;
}

static void sign_n_bits(uint16_t* s, unsigned int n)
{
  if(*s > pow(2, n-1) - 1)
  {
    *s -= pow(2, n);
  }
}

RiSC_VM* vm_init(char filename[])
{
  FILE* file = fopen(filename, "r");
  if(file == NULL)
  {
    ERROR("\tCould not open file \"%s\".\n", filename);
  }

  RiSC_VM* vm = malloc(sizeof *vm);
  if(vm == NULL)
  {
    ERROR("\t%s", OUT_OF_MEMORY);
  }

  memset(vm->regs, 0, sizeof vm->regs);
  vm->regs[7] = STACK_BOTTOM;

  if(print_output)
  {
    printf("Loading values from file \"%s\" ... ", filename);
  }
  int num_lines = load_from_file(vm->program, file);
  if(print_output)
  {
    printf("%d lines loaded from \"%s\".\n\n", num_lines, filename);
  }
  rewind(file);
  fclose(file);

  data_t* d = &vm->data;
  d->data_size = vm->program[0];
  d->data_start = 1;
  d->text_size = vm->program[d->data_start + d->data_size];
  d->text_header = d->data_start + d->data_size;
  d->text_start = d->text_header + 1;

  vm->pc = d->text_start;
  vm->running = true;

  return vm;
}

void vm_shutdown(RiSC_VM* vm)
{
  if(vm != NULL)
  {
    free(vm);
  }
}

bool vm_running(RiSC_VM* vm)
{
  return vm->running;
}

void vm_print_data(RiSC_VM* vm)
{
  for(int i = 0; i < vm->data.data_size; ++i)
  {
    printf("Data[ %2d ] = "PRINT_FORMAT"\n", i, vm->program[vm->data.data_start+i]);
  }
}

void vm_print_regs(RiSC_VM* vm)
{
  uint16_t* r = vm->regs;

	printf
	(
	"+------------+------------+------------+------------+\n"
	"| " KRED "r0" RESET ": " KGRN TABLE_PRINT_FORMAT RESET " "
	"| " KRED "r1" RESET ": " KGRN TABLE_PRINT_FORMAT RESET " "
	"| " KRED "r2" RESET ": " KGRN TABLE_PRINT_FORMAT RESET " "
	"| " KRED "r3" RESET ": " KGRN TABLE_PRINT_FORMAT RESET " |\n"
	"+------------+------------+------------+------------+\n"
	"| " KRED "r4" RESET ": " KGRN TABLE_PRINT_FORMAT RESET " "
	"| " KRED "r5" RESET ": " KGRN TABLE_PRINT_FORMAT RESET " "
	"| " KRED "r6" RESET ": " KGRN TABLE_PRINT_FORMAT RESET " "
	"| " KRED "r7" RESET ": " KGRN TABLE_PRINT_FORMAT RESET " |\n"
	"+------------+------------+------------+------------+\n"
	"| " KRED "pc" RESET ": " KGRN TABLE_PRINT_FORMAT RESET " |\n"
	"+------------+\n",
	r[0], r[1], r[2], r[3],
	r[4], r[5], r[6], r[7],
	vm->pc
	);
}

void vm_fetch(RiSC_VM* vm)
{
  if(vm->pc >= vm->data.text_header + vm->data.text_size)
  {
    vm->running = false;
  }
  vm->pc += 1;
}

void vm_decode(RiSC_VM* vm)
{
  uint16_t instr = vm->program[vm->pc - 1];
  uint16_t opcode = (instr & MASK_OPCODE) >> (16-3);
  uint16_t reg0 = (instr & MASK_REG_A) >> (16-6);
  uint16_t reg1 = (instr & MASK_REG_B) >> (16-9);
  uint16_t reg2 = (instr & MASK_REG_C);
  uint16_t simm_value = (instr & MASK_SIMM);
  uint16_t uimm_value = (instr & MASK_UIMM);

  if(print_output)
  {
    char binary_buffer[17];
    printf("%s\n", decimal_to_binary(binary_buffer, instr, 16));
  }

  sign_n_bits(&simm_value, 7);

  if(reg0 == 0)
  {
    vm->regs[reg0] = 0;
  }
  if(reg1 == 0)
  {
    vm->regs[reg1] = 0;
  }
  if(reg2 == 0)
  {
    vm->regs[reg2] = 0;
  }

  vm->current_instruction = (instruction_t) {opcode, reg0, reg1, reg2, simm_value, uimm_value};
}

void vm_execute(RiSC_VM* vm)
{
  uint16_t opcode = vm->current_instruction.opcode;
  uint16_t reg0 = vm->current_instruction.reg0;
  uint16_t reg1 = vm->current_instruction.reg1;
  uint16_t reg2 = vm->current_instruction.reg2;
  uint16_t simm_value = vm->current_instruction.simm;
  uint16_t uimm_value = vm->current_instruction.uimm;

  switch(opcode)
  {
    case ADD:
      vm->regs[reg0] = vm->regs[reg1] + vm->regs[reg2];
      if(print_output)
      {
        printf("add r%d, r%d, r%d\n", reg0, reg1, reg2);
      }
      break;

    case ADDI:
      vm->regs[reg0] = vm->regs[reg1] + simm_value;
      if(print_output)
      {
        printf("addi r%d, r%d, "PRINT_FORMAT"\n", reg0, reg1, simm_value);
      }
      break;

    case NAND:
      vm->regs[reg0] = ~(vm->regs[reg1] & vm->regs[reg2]);
      if(print_output)
      {
        printf("nand r%d, r%d, r%d\n", reg0, reg1, reg2);
      }
      break;

    case LUI:
      vm->regs[reg0] = uimm_value << 6;
      if(print_output)
      {
        printf("lui r%d, "PRINT_FORMAT"\n", reg0, uimm_value);
      }
      if((vm->regs[reg0] & 0x3F) != 0)
      {
        printf("%s: %s: LUI: Something went wrong!\n", __FILE__, __func__);
      }
      break;

    case SW:
      vm->program[vm->regs[reg1] + simm_value] = vm->regs[reg0];
      if(print_output)
      {
        printf("sw r%d, r%d, "PRINT_FORMAT"\n", reg0, reg1, simm_value);
      }
      break;

    case LW:
      if(print_output)
      {
        printf("lw r%d, r%d, "PRINT_FORMAT"\n", reg0, reg1, simm_value);
      }
      vm->regs[reg0] = vm->program[vm->regs[reg1] + simm_value];
      break;

    case BEQ:
      if(vm->regs[reg0] == vm->regs[reg1])
      {
        vm->pc += simm_value;
        if(print_output)
        {
          printf("<< Equal contents >>\n");
        }
      }
      if(print_output)
      {
        printf("beq r%d, r%d, "PRINT_FORMAT"\n", reg0, reg1, simm_value);
      }
      break;

    case JALR:
      vm->regs[reg0] = vm->pc;
      vm->pc = vm->regs[reg1];
      if(print_output)
      {
        printf("jalr r%d, r%d\n", reg0, reg1);
      }
      break;
  }
}
