#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/termios.h>
#include <sys/mman.h>

enum
{
  R_R0 = 0,
  R_R1,
  R_R2,
  R_R3,
  R_R4,
  R_R5,
  R_R6,
  R_R7,
  R_PC,
  R_COND,
  R_COUNT
};

enum
{
    POS_FL = 1 << 0,
    ZRO_FL = 1 << 1,
    NEG_FL = 1 << 2,
};

enum
{
    OP_BR = 0,
    OP_ADD,
    OP_LD,
    OP_ST,
    OP_JSR,
    OP_AND,
    OP_LDR,
    OP_STR,
    OP_RTI,
    OP_NOT,
    OP_LDI,
    OP_STI,
    OP_JMP,
    OP_RES,
    OP_LEA,
    OP_TRAP
  };

  enum
  {
    MR_KBSR = 0xFE00,
    MR_KBDR = 0xFE02
  };

  enum
  {
    TRAP_GETC = 0x20,
    TRAP_OUT = 0x21,
    TRAP_PUTS = 0x22,
    TRAP_IN = 0x23,
    TRAP_PUTSP = 0x24,
    TRAP_HALT = 0x25
  };

  uint16_t memory_locations[UINT16_MAX];
  uint16_t reg[R_COUNT];

  uint16_t sign_extension(uint16_t x, int bit_count)
{
    if ((x >> (bit_count - 1)) & 1) {
        x |= (0xFFFF << bit_count);
    }
    return x;
}

uint16_t swap16(uint16_t x)
{
    return (x << 8) | (x >> 8);
}

void update_cond_flags(uint16_t r)
{
  if(reg[r] == 0)
  {
    reg[R_COND] = ZRO_FL;
  }
  else if(reg[r] >> 15)
  {
    reg[R_COND] = NEG_FL;
  }
  else
  {
    reg[R_COND] = POS_FL;
  }
}

void read_image_file(FILE* file)
{
  uint16_t origin;
  fread(&origin, sizeof(origin), 1, file);
  origin = swap16(origin);

  uint16_t max_read = UINT16_MAX - origin;
  uint16_t* p = memory_locations + origin;
  size_t read = fread(p, sizeof(uint16_t), max_read, file);

  while(read-- > 0)
  {
    *p = swap16(*p);
    ++p;
  }
}

int read_image(const char* image_path)
{
  FILE* file = fopen(image_path, "rb");
  if(!file)
  {
    return 0;
  }
  read_image_file(file);
  fclose(file);
  return 1;
}

uint16_t check_key()
{
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(STDIN_FILENO, &readfds);

    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;
    return select(1, &readfds, NULL, NULL, &timeout) != 0;
}

void mem_write(uint16_t address, uint16_t val)
{
  memory_locations[address] = val;
}

uint16_t mem_read(uint16_t address)
{
  if(address == MR_KBSR)
  {
    if(check_key())
    {
      memory_locations[MR_KBSR] = (1 << 15);
      memory_locations[MR_KBDR] = getchar();
    }
    else
    {
      memory_locations[MR_KBSR] = 0;
    }
  }
  return memory_locations[address];
}

struct termios original_tio;

void disable_input_buffering()
{
    tcgetattr(STDIN_FILENO, &original_tio);
    struct termios new_tio = original_tio;
    new_tio.c_lflag &= ~ICANON & ~ECHO;
    tcsetattr(STDIN_FILENO, TCSANOW, &new_tio);
}

void restore_input_buffering()
{
    tcsetattr(STDIN_FILENO, TCSANOW, &original_tio);
}

/* Handle Interrupt */
void handle_interrupt(int signal)
{
    restore_input_buffering();
    printf("\n");
    exit(-2);
}

int machine_running = 1;
template <unsigned op>

void instruction(uint16_t instr)
{
  uint16_t r0, r1, r2, imm5, imm_flag;
  uint16_t pc_plus_offset, base_plus_offset;

  constexpr uint16_t opbit = (1 << op);
  if(0x4EEE & opbit)
  {
    r0 = (instr >> 9) & 0x7;
  }
  if(0x12F3 & opbit)
  {
    r1 = (instr >> 6) & 0x7;
  }
  if(0x0022 & opbit)
  {
    imm_flag = (instr >> 5) & 0x1;

    if(imm_flag)
    {
      imm5 = sign_extension(instr & 0x1F, 5);
    }
    else
    {
      r2 = instr & 0x7;
    }
  }
  if(0x00C0 & opbit)
  {
    base_plus_offset = reg[r1] + sign_extension(instr & 0x3F, 6);
  }
  if(0x4C0D & opbit)
  {
    pc_plus_offset = reg[R_PC] + sign_extension(instr & 0x1FF, 9);
  }
  if(0x0001 & opbit)
  {
    uint16_t conditional_ = (instr >> 9) & 0x7;
    if(conditional_ & reg[R_COND])
    {
      reg[R_PC] = pc_plus_offset;
    }
  }
  if(0x0002 & opbit)
  {
    if(imm_flag)
    {
      reg[r0] = reg[r1] + imm5;
    }
    else
    {
      reg[r0] = reg[r1] + reg[r2];
    }
  }
  if(0x0200 & opbit)
  {
    reg[r0] = ~reg[r1];
  }
  if(0x1000 & opbit)
  {
    reg[R_PC] = reg[r1];
  }
  if(0x0010 & opbit)
  {
    uint16_t long_flag = (instr >> 11) & 1;
    reg[R_R7] = reg[R_PC];
    if(long_flag)
    {
      pc_plus_offset = reg[R_PC] + sign_extension(instr & 0x7FF, 11);
      reg[R_PC] = pc_plus_offset;
    }
    else
    {
      reg[R_PC] = reg[r1];
    }
  }
  if(0x0004 & opbit)
  {
    reg[r0] = mem_read(pc_plus_offset);
  }
  if(0x0400 & opbit)
  {
    reg[r0] = mem_read(mem_read(pc_plus_offset));
  }
  if(0x0040 & opbit)
  {
    reg[r0] = mem_read(base_plus_offset);
  }
  if(0x4000 & opbit)
  {
    reg[r0] = base_plus_offset;
  }
  if(0x0008 & opbit)
  {
    mem_write(pc_plus_offset, reg[r0]);
  }
  if(0x0800 & opbit)
  {
    mem_write(mem_read(pc_plus_offset), reg[r0]);
  }
  if(0x0080 & opbit)
  {
    mem_write(base_plus_offset, reg[r0]);
  }
  if(0x8000 & opbit)
  {
    switch(instr & 0xFF)
    {
      case TRAP_GETC:
        reg[R_R0] = (uint16_t)getchar();
        break;

      case TRAP_OUT:
        putc((char)reg[R_R0], stdout);
        fflush(stdout);
        break;

      case TRAP_PUTS:
        {
          uint16_t* c = memory_locations + reg[R_R0];
          while(*c)
          {
            putc((char)*c, stdout);
            ++c;
          }
          fflush(stdout);
        }
        break;

        case TRAP_IN:
          {
            printf("Enter a character: ");
            char c = getchar();
            putc(c, stdout);
            reg[R_R0] = (uint16_t)c;
          }
          break;


        case TRAP_PUTSP:
          {
            uint16_t*c = memory_locations + reg[R_R0];
            while(*c)
            {
              char char_1 = (*c) & 0xFF;
              putc(char_1, stdout);
              char char_2 = (*c) >> 8;
              if(char_2)
              {
                putc(char_2, stdout);
              }
              ++c;
            }
            fflush(stdout);
          }
          break;

        case TRAP_HALT:
          puts("HALT");
          fflush(stdout);
          machine_running = 0;

          break;
    }
  }
  if(0x4666 & opbit)
  {
    update_cond_flags(r0);
  }
}

static void (*op_table[16])(uint16_t) = {
  instruction<0>, instruction<1>, instruction<2>, instruction<3>,
  instruction<4>, instruction<5>, instruction<6>, instruction<7>,
  NULL, instruction<9>, instruction<10>, instruction<11>,
  instruction<12>, NULL, instruction<14>, instruction<15>
};

int main(int argc, const char* argv[])
{
  if(argc < 2)
  {
    printf("lc3 [image-file1] ...\n");
    exit(2);
  }

  for(int i = 1; i < argc; ++i)
  {
    if(!read_image(argv[i]))
    {
      printf("failed to load image: %s\n", argv[i]);
      exit(1);
    }
  }

  signal(SIGINT, handle_interrupt);
  disable_input_buffering();

  enum {PC_START = 0x3000};
  reg[R_PC] = PC_START;

  while(machine_running)
  {
    uint16_t instr = mem_read(reg[R_PC]++);
    uint16_t op = instr >> 12;
    op_table[op](instr);
  }
  restore_input_buffering();
}
