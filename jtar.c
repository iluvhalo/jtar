#include <stdio.h>
#include <stdlib.h>
#include <dllist.h>
#include <fields.h>
#include <jval.h>
#include <jrb.h>
#include <sys/stat.h>
#include <sys/types.h>

int extractTar(int argc, char **argv, char print);
int createTar(int argc, char **argv, char print);
int process_files(char *file);

int main (int argc, char **argv) {
   int i;
   char gucci, print;

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

   print = 0;
   if ((strcmp(argv[1], "cv") == 0) || (strcmp(argv[1], "xv") == 0)) print = 1;

   if ((strcmp(argv[1], "c") == 0) || (strcmp(argv[1], "cv") == 0)) {
      printf("command is createTar()\n");
      createTar(argc, argv, print);
   } else {
      printf("command is extractTar()\n");
      extractTar(argc, argv, print);
   }

	return 0;
}

int createTar(int argc, char **argv, char print) {
   int i;

   printf("Argument: %d\n", print);

   for (i = 2; i < argc; i++) {
      printf("File: %s\n", argv[i]);
      process_files(argv[i]);
   }
}

int extractTar(int argc, char **argv, char print) {
   int i;

   printf("Argument: %d\n", print);

   for (i = 2; i < argc; i++) {
      printf("File: %s\n", argv[i]);
      process_files(argv[i]);
   }
}

int process_files(char *s) {
   struct stat buf;
   int exists;
   
   printf("Processing file: %s\n", s);

   exists = lstat(s, &buf);

   if (exists < 0) {
      printf("%s does not exist\n", s);
      return -1;
   } else {
      if (S_ISDIR(buf.st_mode)) {
         printf("is a directory\n");
      }
   }

}
