#include <stdio.h>
#include <stdlib.h>
#include <dllist.h>
#include <fields.h>
#include <jval.h>
#include <jrb.h>

int process_files(char *s);

int main (int argc, char **argv) {
   int i;
   char gucci;

   if (argc < 2) {
      fprintf(stderr, "usage: jtar [cxv] [files ...]\n");
      exit(1);
   } 

   gucci = 0;
   if ((strcmp(argv[1], "c") == 0) || (strcmp(argv[1], "x") == 0) || (strcmp(argv[1], "cv") == 0) || (strcmp(argv[1], "xv") == 0)) {
      gucci = 1;
   }
   if (gucci != 1) {
      fprintf(stderr, "usage: jtar [cxv] [files ...]\n");
      exit(1);
   }

	return 0;
}
