#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/termios.h>
#include <sys/mman.h>

/*Registers*/
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

/*Opcodes*/
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

/*Condition Flags*/
enum
{
  POS_FL = 1 << 0,
  ZRO_FL = 1 << 1,
  NEG_FL = 1 << 2,
};

/*Trap Codes*/
enum
{
  TRAP_GETC = 0x20,
  TRAP_OUT = 0x21,
  TRAP_PUTS = 0x22,
  TRAP_IN = 0x23,
  TRAP_PUTSP = 0x24,
  TRAP_HALT = 0x25
};

/*Memory Mapped Registers*/
enum
{
  MR_KBSR = 0xFE00,
  MR_KBDR = 0xFE02
};

/*Memory Locations*/
uint16_t memory_locations[UINT16_MAX];

/*Register Array*/
uint16_t reg[R_COUNT];

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

void handle_interrupt(int signal)
{
    restore_input_buffering();
    printf("\n");
    exit(-2);
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

uint16_t swap16(uint16_t x)
{
  return (x << 8) | (x >> 8);
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

uint16_t sign_extension(uint16_t x, int bit_count)
{
  if((x >> (bit_count - 1)) & 1)
  {
    x |= (0xFFFF << bit_count);
  }
  return x;
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

int main(int argc, const char* argv[])
{

  if(argc < 2)
  {
    printf("lc3 [image-file1]...\n");
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

  int machine_running = 1;

  while(machine_running)
  {
    uint16_t instr = mem_read(reg[R_PC]++);
    uint16_t opcode_ = instr >> 12;

    switch(opcode_)
    {
      case OP_ADD:
      {
        /*Destination Register*/
        uint16_t r0 = (instr >> 9) & 0x7;
        /*First Source Register*/
        uint16_t r1 = (instr >> 6) & 0x7;
        /*Check for immediate mode*/
        uint16_t imm_flag = (instr >> 5) & 0x1;

        if(imm_flag)
        {
          uint16_t imm5_value = sign_extension(instr & 0x1F, 5);
          reg[r0] = reg[r1] + imm5_value;
        }
        else
        {
          uint16_t r2 = instr & 0x7;
          reg[r0] = reg[r1] + reg[r2];
        }

        update_cond_flags(r0);
      }

      break;

      case OP_AND:
      {
        /*Destination Register*/
        uint16_t r0 = (instr >> 9) & 0x7;
        /*First Source Register*/
        uint16_t r1 = (instr >> 6) & 0x7;
        uint16_t imm_flag = (instr >> 5) & 0x1;

        if(imm_flag)
        {
          uint16_t imm5_value = sign_extension(instr & 0x1F, 5);
          reg[r0] = reg[r1] + imm5_value;
        }
        else
        {
          uint16_t r2 = instr & 0x7;
          reg[r0] = reg[r1] + reg[r2];
        }

        update_cond_flags(r0);
      }

      break;

      case OP_NOT:
      {
        /*Destination Register*/
        uint16_t r0 = (instr >> 9) & 0x7;
        /*Source Register*/
        uint16_t r1 = (instr >> 6) & 0x7;

        reg[r0] = ~reg[r1];

        update_cond_flags(r0);
      }

      break;

      case OP_BR:
      {
        uint16_t pc_offset = sign_extension(instr & 0x1FF, 9);
        uint16_t conditional_flag = (instr >> 9) & 0x7;
        if(conditional_flag & reg[R_COND])
        {
          reg[R_PC] += pc_offset;
        }
      }

      break;

      case OP_JMP:
      {
        uint16_t r1 = (instr >> 6) & 0x7;
        reg[R_PC] = reg[r1];
      }

      break;

      case OP_JSR:
      {
        uint16_t long_flag = (instr >> 11) & 1;
        reg[R_R7] = reg[R_PC];
        if(long_flag)
        {
          uint16_t long_pc_offset = sign_extension(instr & 0x7FF, 11);
          reg[R_PC] += long_pc_offset;
        }
        else
        {
          uint16_t r1 = (instr >> 6) & 0x7;
          reg[R_PC] = reg[r1];
        }
      }

      break;

      case OP_LD:
      {
        uint16_t r0 = (instr >> 9) & 0x7;
        uint16_t pc_offset = sign_extension(instr & 0x1FF, 9);
        reg[r0] = mem_read(reg[R_PC] + pc_offset);
        update_cond_flags(r0);
      }

      break;

      case OP_LDR:
      {
        uint16_t r0 = (instr >> 9) & 0x7;
        uint16_t r1 = (instr >> 6) & 0x7;
        uint16_t offset = sign_extension(instr & 0x3F, 6);
        reg[r0] = mem_read(reg[r1] + offset);
        update_cond_flags(r0);
      }

      break;

      case OP_LEA:
      {
        uint16_t r0 = (instr >> 9) & 0x7;
        uint16_t pc_offset = sign_extension(instr & 0x1FF, 9);
        reg[r0] = reg[R_PC] + pc_offset;
        update_cond_flags(r0);
      }

      break;

      case OP_ST:
      {
        uint16_t r1 = (instr >> 9) & 0x7;
        uint16_t pc_offset = sign_extension(instr & 0x1FF, 9);
        mem_write(reg[R_PC] + pc_offset, reg[r1]);
      }

      break;

      case OP_STI:
      {
        uint16_t r1 = (instr >> 9) & 0x7;
        uint16_t pc_offset = sign_extension(instr & 0x1FF, 9);
        mem_write(mem_read(reg[R_PC] + pc_offset), reg[r1]);
      }

      break;

      case OP_STR:
      {
        uint16_t r1 = (instr >> 9) & 0x7;
        uint16_t r2 = (instr >> 6) & 0x7;
        uint16_t offset = sign_extension(instr & 0x3F, 6);
        mem_write(reg[r2] + offset, reg[r1]);
      }

      break;

      case OP_TRAP:
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

      break;

      case OP_RES:
      case OP_RTI:
      default:
        abort();
        break;
    }
  }

  restore_input_buffering();
}
