#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#define MAX_REGS  16
#define SP  13
#define LR  14
#define PC  15
#define STACK_SIZE  1024

struct arm_state
{
  unsigned int regs[MAX_REGS];
  unsigned int cpsr;
  unsigned char* stack;
  unsigned int comp_count;
  unsigned int mem_count;
  unsigned int br_count;
};

struct arm_state* new_arm_state(unsigned int* func, unsigned int arg0, unsigned int arg1, unsigned int arg2, unsigned int arg3)
{
  struct arm_state* arm_s;
  int i;

  arm_s = (struct arm_state*)malloc(sizeof(struct arm_state));
  if(arm_s == NULL)
  {
    printf("Unable to allocate memory failed, exiting.\n");
    exit(-1);
  }

  arm_s->stack = (unsigned char*)malloc(STACK_SIZE);
  if(arm_s->stack == NULL)
  {
    printf("Unable to allocate memory failed, exiting.\n");
    exit(-1);
  }

  arm_s->cpsr = 0;
  for(i = 0; i < MAX_REGS; i++)
  {
    arm_s->regs[i] = 0;
  }

  arm_s->regs[PC] = (unsigned int) func;
  arm_s->regs[SP] = (unsigned int) arm_s->stack + STACK_SIZE;
  arm_s->regs[0] = arg0;
  arm_s->regs[1] = arg1;
  arm_s->regs[2] = arg2;
  arm_s->regs[3] = arg3;

  arm_s->comp_count = 0;
  arm_s->mem_count = 0;
  arm_s->br_count = 0;

  return arm_s;
}

void free_arm_state(struct arm_state* arm_s)
{
  free(arm_s->stack);
  free(arm_s);
}

void print_arm_state(struct arm_state* arm_s, unsigned int sim_result, unsigned int assembler_result)
{
  printf("stack size = %d\n", STACK_SIZE);
  printf("Register values after execution:\n");
  for (int i = 0; i < MAX_REGS; i++)
  {
    printf("r%d = (%X) %d\n", i, arm_s->regs[i], (int) arm_s->regs[i]);
  }
  printf("cpsr: 0x%x\n", arm_s->cpsr);
  printf("Total Instructions Executed: %d\n", (arm_s->comp_count+arm_s->mem_count+arm_s->br_count));
  printf("Total Computational Instructions Executed: %d\n", arm_s->comp_count);
  printf("Total Memory Instructions Executed: %d\n", arm_s->mem_count);
  printf("Total Branch Instructions Executed: %d\n", arm_s->br_count);
  printf("ARM Emulator Result: %d\n", sim_result);
  printf("Assembler Result: %d\n", assembler_result);
}

void set_cpsr_flag(struct arm_state* arm_s, int result, long long result_long)
{
  if (result > 2147483647 || result < -2147483648)
    {
        arm_s->cpsr = arm_s->cpsr | 0x10000000;
    }
    else
    {
        arm_s->cpsr = arm_s->cpsr & 0xEFFFFFFF;
    }

    if(result_long < 0 && result_long > 4294967295)
    {
        arm_s->cpsr = arm_s->cpsr | 0x20000000;
    }
    else
    {
        arm_s->cpsr = arm_s->cpsr & 0xDFFFFFFF;
    }

    if(result < 0)
    {
        arm_s->cpsr = arm_s->cpsr | 0x80000000;
        arm_s->cpsr = arm_s->cpsr & 0xBFFFFFFF;
    }
    else if(result == 0)
    {
        arm_s->cpsr = arm_s->cpsr | 0x40000000;
        arm_s->cpsr = arm_s->cpsr & 0x7FFFFFFF;
    }
    else
    {
        arm_s->cpsr = arm_s->cpsr & 0xBFFFFFFF;
        arm_s->cpsr = arm_s->cpsr & 0x7FFFFFFF;
    }
}

int check_cpsr_flags(struct arm_state* arm_s, unsigned int iw)
{
  unsigned int condition = (iw >> 28) & 0b1111;

  switch(condition)
  {
    case 0:
      return ((arm_s->cpsr >> 30) & 0b1) == 1;

    case 1:
      return ((arm_s->cpsr >> 30) & 0b1) == 0;

    case 11:
      return ((arm_s->cpsr >> 28) &0b1) != ((arm_s->cpsr >> 31) & 0b1);

    case 12:
      return (((arm_s->cpsr >> 30) & 0b1) == 0) && (((arm_s->cpsr >> 31) & 0b1) == ((arm_s->cpsr >> 28) & 0b1));

    case 14:
      return true;
  }
}

bool process_data_instruction(unsigned int iw)
{
  return ((iw >> 26) & 0b11) == 0;
}

bool is_bx_instruction(unsigned int iw)
{
  return ((iw >> 4) & 0xFFFFFF) == 0b000100101111111111110001;
}

void execute_bx_instruction(struct arm_state* arm_s, unsigned int iw)
{
  unsigned int rn = iw & 0b1111;
  arm_s->regs[PC] = arm_s->regs[rn];
}

bool is_branch_instruction(unsigned int iw)
{
  return ((iw >> 25) & 0b111) == 0b101;
}

void execute_branch_instruction(struct arm_state* arm_s, unsigned int iw)
{
  unsigned int signed_bit = (iw >> 23) & 0b1;
  unsigned int l_bit = (iw >> 24) & 0b1;
  unsigned int offset = iw & 0xFFFFFF;
  unsigned int destination;

  if(signed_bit == 0)
  {
    destination = offset | 0x00000000;;
  }
  else
  {
    destination = offset | 0xFF000000;
  }

  destination = destination << 2;

  if(l_bit == 1)
  {
    arm_s->regs[LR] = arm_s->regs[PC] + 4;
  }

  arm_s->regs[PC] += (destination + 8);

}

bool is_data_transfer_instruction(unsigned int iw)
{
  return ((iw >> 26) & 0b11) == 0b01;
}

void execute_data_transfer_instruction(struct arm_state* arm_s, unsigned int iw)
{
  unsigned int rd = (iw>>12) & 0xF;
  unsigned int rn = (iw>>16) & 0xF;
  unsigned int l_bit = (iw>>20) & 0b1;
  unsigned int w_bit = (iw>>21) & 0b1;
  unsigned int b_bit = (iw>>22) & 0b1;
  unsigned int u_bit = (iw>>23) & 0b1;
  unsigned int p_bit = (iw>>24) & 0b1;
  unsigned int i_bit = (iw>>25) & 0b1;
  unsigned int modified_base_value = arm_s->regs[rn];
  unsigned int offset_value;

  if(i_bit == 1)
  {
    offset_value = arm_s->regs[(iw & 0xF)] << ((iw>>7) & 0b11111);
  }
  else
  {
    offset_value = iw & 0xFFF;
  }

  if(b_bit == 1)
  {
    if (l_bit == 1)
    {
      arm_s->regs[rd] = (unsigned int)*((char *)modified_base_value);
    }
  }
  else
  {
    if (l_bit == 1)
    {
      arm_s->regs[rd] = *((unsigned int *) modified_base_value);
    }
    else
    {
      *((unsigned int *) modified_base_value) = arm_s->regs[rd];
    }
  }
  if(p_bit == 0)
  {
    if(u_bit == 1)
    {
      modified_base_value += offset_value;
    }
    else
    {
      modified_base_value += offset_value;
    }
  }
  if(w_bit == 1)
  {
    arm_s->regs[rn] = modified_base_value;
  }
  arm_s->regs[PC] += 4;
}

bool is_push(unsigned int iw)
{
  return (((iw >> 25) & 0b111) == 0b100) && (((iw >> 20) & 0b1) == 0b0);
}

bool is_pop(unsigned int iw)
{
  return (((iw >> 25) & 0b111) == 0b100) && (((iw>>20) & 0b1) == 0b1);
}

void execute_push(struct arm_state* arm_s, unsigned int iw)
{
  unsigned int register_list = iw & 0xFFFF;
  unsigned int rn = (iw>>16) & 0xF;
  unsigned int w_bit = (iw>>21) & 0b1;
  unsigned int s_bit = (iw>>22) & 0b1;
  unsigned int u_bit = (iw>>23) & 0b1;
  unsigned int p_bit = (iw>>24) & 0b1;
  unsigned int modified_base_value = arm_s->regs[rn];
  unsigned int offset_value = 4;

  for(int i = (MAX_REGS - 1); i >= 0; i--)
  {
    if(((register_list >> i) & 0b1) == 0b1)
    {
      if(p_bit == 1)
      {
        if (u_bit == 1)
        {
          modified_base_value += offset_value;
        }
        else
        {
          modified_base_value -= offset_value;
        }
      }
      *((unsigned int *) modified_base_value) = arm_s->regs[i];
      if(p_bit == 0)
      {
        if (u_bit == 1)
        {
          modified_base_value += offset_value;
        }
        else
        {
          modified_base_value -= offset_value;
        }
      }
      if(w_bit == 1)
      {
        arm_s->regs[rn] = modified_base_value;
      }
    }
  }

  arm_s->regs[PC] += 4;
}

void execute_pop(struct arm_state* arm_s, unsigned int iw)
{
  unsigned int register_list = iw & 0xFFFF;
  unsigned int rn = (iw>>16) & 0xF;
  unsigned int w_bit = (iw>>21) & 0b1;
  unsigned int s_bit = (iw>>22) & 0b1;
  unsigned int u_bit = (iw>>23) & 0b1;
  unsigned int p_bit = (iw>>24) & 0b1;
  unsigned int modified_base_value = arm_s->regs[rn];
  unsigned int offset_value = 4;

  for(int i = 0; i < MAX_REGS; i++)
  {
    if(((register_list >> i) & 0b1) == 0b1)
    {
      if(p_bit == 1)
      {
        if(u_bit == 1)
        {
          modified_base_value += offset_value;
        }
        else
        {
          modified_base_value -= offset_value;
        }
      }
      arm_s->regs[i] = *((unsigned int *) (modified_base_value));
      if(p_bit == 0)
      {
        if(u_bit == 1)
        {
          modified_base_value += offset_value;
        }
        else
        {
          modified_base_value -= offset_value;
        }
      }
      if(w_bit == 1)
      {
        arm_s->regs[rn] = modified_base_value;
      }
    }

  }
  arm_s->regs[PC] += 4;
}

void execute_process_data_instruction(struct arm_state* arm_s, unsigned int iw)
{
  unsigned int rm_value;
  unsigned int i_bit = (iw>>25) & 0b1;
  unsigned int opcode = (iw >> 21) & 0xF;
  unsigned int s_bit = (iw >> 20) & 0b1;
  unsigned int rd = (iw >> 12) & 0xF;
  unsigned int rn = (iw >> 16) & 0xF;
  int result;
  long long result_long;

  if(i_bit == 1)
  {
    rm_value = iw & 0xFF;
  }
  else
  {
    rm_value = arm_s->regs[(iw & 0xF)];
  }

  switch(opcode)
  {
    case 2:
      arm_s->regs[rd] = arm_s->regs[rn] - rm_value;
      result_long = (long long) arm_s->regs[rn] - (long long) rm_value;
      result = arm_s->regs[rd];
      break;

    case 4:
      arm_s->regs[rd] = arm_s->regs[rn] + rm_value;
      result_long = (long long) arm_s->regs[rn] + (long long) rm_value;
      result = arm_s->regs[rd];
      break;

    case 10:
      result = arm_s->regs[rn] - rm_value;
      result_long = (long long) arm_s->regs[rn] - (long long) rm_value;
      break;

    case 11:
      result = arm_s->regs[rn] + rm_value;
      result_long = (long long) arm_s->regs[rn] + (long long) rm_value;
      break;

    case 13:
      arm_s->regs[rd] = rm_value;
      result = arm_s->regs[rd];
      result_long = (long long) arm_s->regs[rd];
      break;

    case 15:
      arm_s->regs[rd] = ~(rm_value);
      result = ~(rm_value);
      result_long = (long long) arm_s->regs[rd];
      break;
  }

  if(s_bit == 1)
  {
    set_cpsr_flag(arm_s, result, result_long);
  }
}

void arm_state_first_execute(struct arm_state* arm_s)
{
  unsigned int iw;
  unsigned int *pc;

  pc = (unsigned int *) arm_s->regs[PC];
  iw = *pc;

  if(is_bx_instruction(iw))
  {
    arm_s->br_count++;
    if(check_cpsr_flags(arm_s, iw))
    {
      execute_bx_instruction(arm_s, iw);
    }
  }
  else if(is_branch_instruction(iw))
  {
    arm_s->br_count++;
    if(check_cpsr_flags(arm_s, iw))
    {
      execute_branch_instruction(arm_s, iw);
    }
    else
    {
      arm_s->regs[PC] += 4;
    }
  }
  else if(process_data_instruction(iw))
  {
    arm_s->comp_count++;
    if(check_cpsr_flags(arm_s, iw))
    {
      execute_process_data_instruction(arm_s, iw);
    }
    arm_s->regs[PC] += 4;
  }
  else if(is_data_transfer_instruction(iw))
  {
    arm_s->mem_count++;
    if(check_cpsr_flags(arm_s, iw))
    {
      execute_data_transfer_instruction(arm_s, iw);
    }
  }
  else if(is_push(iw))
  {
    arm_s->mem_count++;
    if(check_cpsr_flags(arm_s, iw))
    {
      execute_push(arm_s, iw);
    }
  }
  else if(is_pop(iw))
  {
    arm_s->mem_count++;
    if(check_cpsr_flags(arm_s, iw))
    {
      execute_pop(arm_s, iw);
    }
  }
}

unsigned int arm_state_execute(struct arm_state* arm_s)
{
  while(arm_s->regs[PC] != 0)
  {
    arm_state_first_execute(arm_s);
  }

  return arm_s->regs[0];
}
