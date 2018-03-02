#include <stdio.h>
#include <stdlib.h>
#include <dllist.h>
#include <jval.h>
#include <jrb.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <string.h>

int extractTar(int argc, char **argv, char print);
int createTar(int argc, char **argv, char print);
int process_files(char *file, Dllist d);

int main (int argc, char **argv) {
   int i;
   char gucci, print;

   // make sure there are enough arguments
   if (argc < 2) {
      fprintf(stderr, "usage: jtar [cxv] [files ...]\n");
      exit(1);
   } 

   // make sure the second argument is a valid command
   gucci = 0;
   if ((strcmp(argv[1], "c") == 0) || (strcmp(argv[1], "x") == 0) || (strcmp(argv[1], "cv") == 0) || (strcmp(argv[1], "xv") == 0)) {
      gucci = 1;
   }
   if (gucci != 1) {
      fprintf(stderr, "usage: jtar [cxv] [files ...]\n");
      exit(1);
   }

   // sets the bool for if to print debug lines to stderr
   print = 0;
   if ((strcmp(argv[1], "cv") == 0) || (strcmp(argv[1], "xv") == 0)) print = 1;

   // checks the command and calls the appropriate function
   if ((strcmp(argv[1], "c") == 0) || (strcmp(argv[1], "cv") == 0)) {
      printf("command is createTar()\n");
      createTar(argc, argv, print);
   } else {
      printf("command is extractTar()\n");
      extractTar(argc, argv, print);
   }

	return 0;
}

// creates the tarfile by calling process_files() on each file in the command line arguments
// it then recursively goes through all the files and subdirectories adding them to a dllist
// Then it iterates through the dllist and adds all the approporate information for each item 
// into their respective dllists (dirs, files, inode, hardlinks)
int createTar(int argc, char **argv, char print) {
   int i;
   Dllist d;

   d = new_dllist();

   printf("Argument: %d\n", print);

   // call process_files() on all the command line files 
   for (i = 2; i < argc; i++) {
      printf("File: %s\n", argv[i]);
      process_files(argv[i], d);
   }
}

int extractTar(int argc, char **argv, char print) {
   int i;
   Dllist d;

   printf("Argument: %d\n", print);

   for (i = 2; i < argc; i++) {
      printf("File: %s\n", argv[i]);
//      process_files(argv[i], d);
   }
}

int process_files(char *s, Dllist d) {
   DIR *dir;
   struct stat buf;
   struct dirent *de;
   int exists;
   char *path;
   char *rPath;
   char p;
   Dllist tmp;
   char *name;
   
   printf("Processing file: %s\n", s);

   exists = lstat(s, &buf);

   if (exists < 0) {
      printf("%s does not exist\n", s);
      return -1;
   } else {
      printf("S_ISDIR: %d\n", S_ISDIR(buf.st_mode));
      printf("S_ISREG: %d\n", S_ISREG(buf.st_mode));
      if (S_ISDIR(buf.st_mode)) {
         printf("%s is a directory\n", s);
         
         dir = opendir(s);
         if (dir == NULL) {
            perror("prsize");
            exit(1);
         }

         path = (char *) malloc((strlen(s) + 258) * sizeof(char));

         // iterate through the contents of the directory and add them to the dllist
         for (de = readdir(dir); de != NULL; de = readdir(dir)) {
            
            sprintf(path, "%s/%s", s, de->d_name);
            printf("    File: %s\n", de->d_name);
            printf("    path: %s\n", path);
            
            rPath = strdup(path);

            exists = lstat(rPath, &buf);
            
            printf("S_ISDIR: %d\n", S_ISDIR(buf.st_mode));
            printf("S_ISREG: %d\n", S_ISREG(buf.st_mode));
            if (S_ISDIR(buf.st_mode)) {
               printf("    %s is a directory\n", rPath);
            } else {
               printf("    %s is a file\n", rPath);
            }

            if (exists < 0) {

               fprintf(stderr, "Couldn't stat %s\n", rPath);
               exit(1);

            } else if ((strcmp(de->d_name, ".") != 0) && (strcmp(de->d_name, "..") != 0)) {
               // ignore . and ..
               printf("    try to add to dllist\n");

               // check if it already in the dllist
               p = 0;
               dll_traverse(tmp, d) {
                  if (strcmp(tmp->val.s, rPath) == 0) {
                     p = 1;
                     printf("  already in dllist\n    tmp->val.s: %s    path: %s\n", tmp->val.s, rPath);
                  }
               }

               // if not, add it
               if (p == 0) {
                  dll_append(d, new_jval_s(rPath));
                  printf("   adding %s to the dllist\n", rPath);
               }

            } else {
               printf("   %s is skipped over\n", de->d_name);
            }
         }

         closedir(dir);

         // traverse the dllist and call process_files on everything
         dll_traverse(tmp, d) {
            name = strdup(tmp->val.s);

            printf("calling process_files() on %s\n", name);
            dll_delete_node(tmp);
            process_files(name, d);
         }
      } else if (S_ISREG(buf.st_mode)) {
         printf("%s is a file\n", s);
         printf("   do nothing\n");
         return;
      } else {
         printf("I SHOULD NOT SEE THIS AT ALL                        \n");
         printf("              _                                     \n");
         printf("         .___(.)<  'YOU DUN FUCKED UP REAL GOOD'    \n");
         printf("          \\____)                                   \n");
         printf("~~~~~~~~~~~~~~~~~~~~~~~~~~~~                        \n");
      }
   }
}
