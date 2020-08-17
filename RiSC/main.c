#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

#include "virtual_machine.h"

#define EXIT_MESSAGE  "Program exited successfully.\n"

bool step_through_program;
bool print_output;

int main(int argc, char* argv[])
{
  if(argc < 2)
  {
    printf("Usage: run <input_filename>\n");
    exit(EXIT_FAILURE);
  }

  char* program_name = argv[1];

  for(int i = 2; i < argc; ++i)
  {
    if(!strcmp(argv[i], "--step"))
    {
      step_through_program = true;
    }
    else if(!strcmp(argv[i], "--verbose"))
    {
      print_output = true;
    }
    else
    {
      printf("Error: Unknown selection \"%s\". Available " "options are:\n" " --step  Step through the program.\n" "  --verbose Print more information.\n", argv[i]);
      exit(EXIT_FAILURE);
    }
  }

  printf("Welcome to the RiSC Virtual Machine");

  RiSC_VM* vm = vm_init(program_name);

  while(vm_running(vm))
  {
    vm_fetch(vm);
    vm_decode(vm);
    vm_execute(vm);

    if(step_through_program)
    {
      vm_print_regs(vm);
      vm_print_data(vm);
      printf("[PRESS ENTER]");
      getchar();
      printf("\n");
    }
  }

  if(!step_through_program)
  {
    vm_print_regs(vm);
    vm_print_data(vm);
  }

  vm_shutdown(vm);
  vm = NULL;

  printf(EXIT_MESSAGE);
  return EXIT_SUCCESS;
}
