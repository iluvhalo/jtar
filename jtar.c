#include <stdio.h>
#include <stdlib.h>
#include <dllist.h>
#include <jval.h>
#include <jrb.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <limits.h>
#include <string.h>
#include <unistd.h>

int extractTar(int argc, char **argv, char print);
int createTar(int argc, char **argv, char print);
int process_files(char *file, Dllist d, Dllist p, char print);

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

   if (strncmp(argv[2], "..", 2) == 0) {
      fprintf(stderr, "usage: jtar [cxv] [files ...]\n");
      exit(1);
   }

   // sets the bool for if to print debug lines to stderr
   print = 0;
   if ((strcmp(argv[1], "cv") == 0) || (strcmp(argv[1], "xv") == 0)) print = 1;

   // checks the command and calls the appropriate function
   if ((strcmp(argv[1], "c") == 0) || (strcmp(argv[1], "cv") == 0)) {
      if (print) printf("command is createTar()\n");
      createTar(argc, argv, print);
   } else {
      if (print) printf("command is extractTar()\n");
      extractTar(argc, argv, print);
   }

	return 0;
}

// creates the tarfile by calling process_files() on each file in the command line arguments
// it then recursively goes through all the files and subdirectories adding them to a dllist
// Then it iterates through the dllist and adds all the approporate information for each item 
// into their respective dllists (dirs, files, inode, hardlinks)
int createTar(int argc, char **argv, char print) {
   int i, exists;
   Dllist tmp, contents, p;
   JRB t, dirs, files, inode, hardLinks, realPath;
   char *fileName, *filePath;
   struct stat buf, *bufp;
   ino_t inodeNumber;

   contents = new_dllist();
   p = new_dllist();

//   if (print) fprintf(stderr, "Print argument: %d\n", print);

   // call process_files() on all the command line files 
   for (i = 2; i < argc; i++) {
      if (strcmp( (argv[i] + strlen(argv[i]) - 1), "/") == 0) argv[i][strlen(argv[i]) - 1] = '\0';
//      if (print) fprintf(stderr, "File: %s\n", argv[i]);
      dll_append(p, new_jval_s(argv[i]));
      dll_append(contents, new_jval_s(argv[i]));
//      if (print) fprintf(stderr, "Adding file %s to dllist, p\n", argv[i]);
      process_files(argv[i], contents, p, print);
   }

   // print contents of dllist
   if (print) {
      fprintf(stderr, "dllist contents:\n");
      dll_traverse(tmp, contents) fprintf(stderr, "  %s\n", tmp->val.s);
      fprintf(stderr, "dllist P:\n");
      dll_traverse(tmp, p) fprintf(stderr, "  %s\n", tmp->val.s);
   }
   free_dllist(p);

   t = make_jrb(); dirs = make_jrb(); files = make_jrb(); inode = make_jrb(); hardLinks = make_jrb(); realPath = make_jrb();

   // traverse contents and push all the information into jrb's
   dll_traverse(tmp, contents) {
      fileName = tmp->val.s;
      filePath = realpath(tmp->val.s, NULL);
      if (print) fprintf(stderr, "fileName: %s\n", fileName);

      exists = lstat(fileName, &buf);

      if (exists < 0) {
         fprintf(stderr, "File doesn not exist\n");
      } else {
         if (S_ISDIR(buf.st_mode)) {
            //is a directory
            if (print) fprintf(stderr, "   Inserting %s into jrb dirs and jrb realPath\n", fileName);
            jrb_insert_str(dirs, fileName, new_jval_v(buf));
            jrb_insert_str(realPath, fileName, new_jval_s(fileName));
         } else if (S_ISREG(buf.st_mode)) {
            // is a file
            if (print) fprintf(stderr, "   Check if in inode jrb\n");
            inodeNumber = buf.st_ino;
            if (print) fprintf(stderr, "   inodeNumber: %d\n", inodeNumber);
            if (jrb_find_int(inode, inodeNumber) == NULL) {
               if (print) fprintf(stderr, "   %s if a file\n", fileName);
               if (print) fprintf(stderr, "   Inserting %s into jrb files, inode, and realPath\n", fileName);
               jrb_insert_int(inode, inodeNumber, new_jval_i(inodeNumber));
               jrb_insert_str(realPath, fileName, new_jval_s(filePath));
               jrb_insert_str(files, fileName, new_jval_s(fileName));
            } else {
               if (print) fprintf(stderr, "   %s is a hardlink\n", fileName);
               if (print) fprintf(stderr, "   Inserting %s into jrb realPath and hardLinks\n", fileName);
               jrb_insert_str(realPath, fileName, new_jval_s(filePath));
               jrb_insert_str(hardLinks, fileName, new_jval_s(tmp->val.s));
            }
         } else {
            fprintf(stderr, "Something happened.\n");
         }
      }
   } // end of pushing into jrb's

   // treverse jrbs
   jrb_traverse(t, dirs) {
      fileName = t->key.s;
      stat(fileName, &buf);
      printf("%s\n", fileName);
      printf("%lld\n", buf.st_size);
      fwrite(&buf, sizeof(struct stat), 1, stdout);
      printf("\n");
   }
   jrb_traverse(t, files) {
      fileName = t->val.s;
      stat(fileName, &buf);
      printf("%s\n", fileName);
      printf("%lld\n", buf.st_size);
      fwrite(&buf, sizeof(struct stat), 1, stdout);
      printf("\n");
      remove(fileName);
   }
   jrb_traverse(t, hardLinks) {
      fileName = t->key.s;
      stat(fileName, &buf);
      printf("%s\n", fileName);
      printf("%lld\n", buf.st_size);
      fwrite(&buf, sizeof(struct stat), 1, stdout);
      printf("\n");
      remove(fileName);
   }
   jrb_rtraverse(t, dirs) {
      fileName = t->key.s;
      remove(fileName);
   }
} // end of createTar

int extractTar(int argc, char **argv, char print) {
   int i;
   JRB tmp, dirs, files, hardLinks, inode;
   char line[1000];
   char *fileName;
   off_t fileSize;
   struct stat buf;

   memset(line, 0, 1000 * sizeof(char));
   while (scanf("%s\n%lld", fileName, fileSize)) {
      printf("Read Name: %s\nRead Size: %lld", fileName, fileSize);
      read(stdin, &buf, sizeof(buf));
      memset(line, 0, 1000 * sizeof(char));
   }
}

int process_files(char *s, Dllist d, Dllist p, char print) {
   DIR *dir;
   struct stat buf;
   struct dirent *de;
   int exists;
   char *path;
   char *rPath;
   char p1;
   Dllist tmp, ptmp;
   char *name;
   
//   if (print) fprintf(stderr, "Processing file: %s\n", s);

   // my fuck up insurance
   if (strcmp(s + strlen(s) - 6, "jtar.c") == 0) {
      fprintf(stderr, "Don't call jtar on a directory connected to jtar.c ya biscuit-head.\n");
      exit(1);
   }

   exists = lstat(s, &buf);

   if (exists < 0) {
      printf("%s does not exist\n", s);
      return -1;
   } else {
//      if (print) fprintf(stderr, "S_ISDIR: %d\n", S_ISDIR(buf.st_mode));
//      if (print) fprintf(stderr, "S_ISREG: %d\n", S_ISREG(buf.st_mode));
      if (S_ISDIR(buf.st_mode)) {
//         if (print) fprintf(stderr, "  %s is a directory\n", s);
         
         dir = opendir(s);
         if (dir == NULL) {
            perror("prsize");
            exit(1);
         }

         path = (char *) malloc((strlen(s) + 258) * sizeof(char));

         // iterate through the contents of the directory and add them to the dllist
         for (de = readdir(dir); de != NULL; de = readdir(dir)) {
            
            sprintf(path, "%s/%s", s, de->d_name);
//            if (print) fprintf(stderr, "    File: %s\n", de->d_name);
//            if (print) fprintf(stderr, "    path: %s\n", path);
            rPath = strdup(path);
            exists = lstat(rPath, &buf);
            
//            if (print) fprintf(stderr, "    S_ISDIR: %d\n", S_ISDIR(buf.st_mode));
//            if (print) fprintf(stderr, "    S_ISREG: %d\n", S_ISREG(buf.st_mode));
            if (S_ISDIR(buf.st_mode)) {
//               if (print) fprintf(stderr, "    %s is a directory\n", rPath);
            } else {
//               if (print) fprintf(stderr, "    %s is a file\n", rPath);
            }

            if (exists < 0) {
               fprintf(stderr, "Couldn't stat %s\n", rPath);
               exit(1);
            } else if ((strcmp(de->d_name, ".") != 0) && (strcmp(de->d_name, "..") != 0)) {
               // ignore . and ..
//               if (print) fprintf(stderr, "    try to add to dllist\n");

               // check if it already in the dllist
               p1 = 0;
               dll_traverse(tmp, d) {
                  if (strcmp(tmp->val.s, rPath) == 0) {
                     p1 = 1;
                     fprintf(stderr, "      already in dllist\n    tmp->val.s: %s    path: %s\n", tmp->val.s, rPath);
                  }
               }

               // if not, add it
               if (p1 == 0) {
                  dll_append(d, new_jval_s(rPath));
//                  if (print) fprintf(stderr, "    adding %s to the dllist, d\n", rPath);
               }

            } else {
//               if (print) fprintf(stderr, "    %s is skipped over\n", de->d_name);
            }
         }

         closedir(dir);

         // traverse the dllist and call process_files on everything
         dll_traverse(tmp, d) {
            p1 = 0;
            dll_traverse(ptmp, p) {
               if (strcmp(ptmp->val.s, tmp->val.s) == 0) p1 = 1;
            }
            if ((tmp != NULL) && (p1 == 0)) {
               name = strdup(tmp->val.s);

               dll_append(p, new_jval_s(name));
//               if (print) fprintf(stderr, "    adding %s to the dllist, d\n", name);

//               if (print) fprintf(stderr, "    calling process_files() on %s\n", name);
               process_files(name, d, p, print);
            }
         }
      } else if (S_ISREG(buf.st_mode)) {
//         if (print) fprintf(stderr, "  %s is a file\n", s);
//         if (print) fprintf(stderr, "  do nothing\n");
         return;
      } else {
         fprintf(stderr, "I SHOULD NOT SEE THIS AT ALL                        \n");
         fprintf(stderr, "              _                                     \n");
         fprintf(stderr, "         .___(.)<  'YOU DUN FUCKED UP REAL GOOD'    \n");
         fprintf(stderr, "          \\____)                                   \n");
         fprintf(stderr, "~~~~~~~~~~~~~~~~~~~~~~~~~~~~                        \n");
      }
   }
}
