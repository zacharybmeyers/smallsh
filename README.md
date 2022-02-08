Use the Makefile to create an executable for smallsh with

  `make`

To clean up before a re-compile, make use of 

  `make clean`

Alternatively, use gcc with the following flags in bash to create an executable from smallsh.c

  `gcc -std=c99 -g -Wall -o smallsh smallsh.c`

Use the following to run the executable

  `./smallsh`
