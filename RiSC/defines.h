#ifndef DEFINES_H
#define DEFINES_H

#define PRINT_DEC_SIGN  1
#define PRINT_DEC_USIGN 0
#define PRINT_HEX 0
#define TABLE_PRINT_DEC_SIGN  1
#define TABLE_PRINT_DEC_USIGN 0
#define TABLE_PRINT_HEX 0

#define OUT_OF_MEMORY "Out of memory.\n"

#define ERROR(...)							    \
do {									    \
	fprintf(stderr, "Error in file \"%s\", line %d, function \"%s\":\n",\
			__FILE__, __LINE__, __func__);			    \
	fprintf(stderr, __VA_ARGS__);					    \
	exit(EXIT_FAILURE);						    \
} while (0);

#if PRINT_HEX
  #define PRINT_FORMAT  "0x%04x"
#elif PRINT_DEC_SIGN
  #define PRINT_FORMAT  "%"PRId16
#elif PRINT_DEC_USIGN
  #define PRINT_FORMAT  "%"PRIu16
#else
  #define PRINT_FORMAT  <<ERROR: PRINT_FORMAT is undefined>>
#endif

#if TABLE_PRINT_HEX
  #define TABLE_PRINT_FORMAT  "0x%04x"
#elif TABLE_PRINT_DEC_SIGN
  #define TABLE_PRINT_FORMAT  "%6"PRId16
#elif TABLE_PRINT_DEC_USIGN
  #define TABLE_PRINT_FORMAT  "%6"PRIu16
#else
  #define TABLE_PRINT_FORMAT  <<ERROR: TABLE_PRINT_FORMAT is undefined>>
#endif

#define KNRM  "\x1B[0m"
#define KRED  "\x1B[31m"
#define KGRN	"\x1B[32m"
#define KYEL	"\x1B[33m"
#define KBLU	"\x1B[34m"
#define KMAG	"\x1B[35m"
#define KCYN	"\x1B[36m"
#define KWHT	"\x1B[37m"
#define RESET	"\033[0m"

#endif
