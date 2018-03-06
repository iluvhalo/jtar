#include <stdio.h>
#include <fields.h>
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
#include <fcntl.h>
#include <utime.h>

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
   char *file;
   struct stat buf, *bufp;
   ino_t inodeNumber;
   int fd;
   FILE *f;

   if (strncmp(argv[2], "..", 2) == 0) {
      fprintf(stderr, "usage: jtar [cxv] [files ...]\n");
      exit(1);
   }

   contents = new_dllist();
   p = new_dllist();

   // call process_files() on all the command line files 
   for (i = 2; i < argc; i++) {
      if (strcmp( (argv[i] + strlen(argv[i]) - 1), "/") == 0) argv[i][strlen(argv[i]) - 1] = '\0';
      dll_append(p, new_jval_s(argv[i]));
      dll_append(contents, new_jval_s(argv[i]));
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

      exists = lstat(fileName, &buf);

      if (exists < 0) {
         fprintf(stderr, "File doesn not exist\n");
      } else {
         if (S_ISDIR(buf.st_mode)) {
            //is a directory
            jrb_insert_str(dirs, fileName, new_jval_v(buf));
            jrb_insert_str(realPath, fileName, new_jval_s(fileName));
         } else if (S_ISREG(buf.st_mode)) {
            // is a file
            inodeNumber = buf.st_ino;
            if (jrb_find_int(inode, inodeNumber) == NULL) {
               jrb_insert_int(inode, inodeNumber, new_jval_i(inodeNumber));
               jrb_insert_str(realPath, fileName, new_jval_s(filePath));
               jrb_insert_str(files, fileName, new_jval_s(fileName));
            } else {
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
      lstat(fileName, &buf);
      printf("%d", strlen(fileName));
      fwrite(fileName, sizeof(char), strlen(fileName), stdout);
      fwrite(&buf, sizeof(struct stat), 1, stdout);
   }
   jrb_traverse(t, files) {
      fileName = t->val.s;
      lstat(fileName, &buf);
      
      f = fopen(fileName, "r");
      if (f == NULL) {
         fprintf(stderr, "Problem opening the file\n");
      } else {
         printf("%d", strlen(fileName));
         fwrite(fileName, sizeof(char), strlen(fileName), stdout);
         fwrite(&buf, sizeof(struct stat), 1, stdout);
         file = malloc(buf.st_size);
         while(fread(file, 1, buf.st_size, f) > 0) {
            fwrite(file, 1, buf.st_size, stdout);
         }
         if (file) free(file);

         fclose(f);
      }
      //printf(" ");
      remove(fileName);
      //free(file);
   }
   jrb_traverse(t, hardLinks) {
      fileName = t->key.s;
      lstat(fileName, &buf);

      f = fopen(fileName, "r");
      if (f == NULL) {
         fprintf(stderr, "Problem opening file\n");
      } else {
         printf("%d", strlen(fileName));
         fwrite(fileName, sizeof(char), strlen(fileName), stdout);
         fwrite(&buf, sizeof(struct stat), 1, stdout);
         file = malloc(buf.st_size);
         while(fread(file, 1, buf.st_size, f) > 0) {
            fwrite(file, 1, buf.st_size, stdout);
         }
         if (file) free(file);
         fclose(f);
      }
      remove(fileName);
   }
   jrb_rtraverse(t, dirs) {
      fileName = t->key.s;
      remove(fileName);
   }
} // end of createTar

int extractTar(int argc, char **argv, char print) {
   IS is;
   JRB tmp, dirs, inode;
   char *fileName, *file;
   char *linkName;
   int fileNameLen;
   int fileSize;
   struct stat buf, *bufp;
   int fd;
   ino_t inodeNumber;
   FILE *f;
   struct utimbuf time;

   dirs = make_jrb(); inode = make_jrb();

   is = new_inputstruct(NULL);

   while (fscanf(stdin, "%d", &fileNameLen)) {
      memset(&buf, 0, sizeof(struct stat));
      fileName = calloc(fileNameLen+1, sizeof(char));
      fread(fileName, sizeof(char), fileNameLen, stdin);
      if (strcmp(fileName, "") == 0) break;
      
      printf("Read fileNameLen: %d\n", fileNameLen);
      printf("Read Name: %s\n", fileName);
      
      fread(&buf, sizeof(struct stat), 1, stdin);
      printf("st_size: %lld\n", buf.st_size);
      
      if (S_ISDIR(buf.st_mode)) {
         printf("%s is a directory\n", fileName);
         
         mkdir(fileName, 0777);
         bufp = malloc(sizeof(struct stat));
         memcpy(bufp, &buf, sizeof(struct stat));
         jrb_insert_str(dirs, strdup(fileName), new_jval_v(bufp));
      
      } else if (S_ISREG(buf.st_mode)) {
         printf("%s is a regular file.\nRead contents of file\n", fileName);
         
         if (jrb_find_int(inode, buf.st_ino) == NULL) {
            printf("   not found in inode\n");
            jrb_insert_int(inode, buf.st_ino, new_jval_s(fileName));

            f = fopen(fileName, "w");
            if (f == NULL) {
               fprintf(stderr, "File cannot be opened for writing\n");
               exit(1);
            }

            file = malloc(buf.st_size);
            fread(file, sizeof(char), buf.st_size, stdin);
            fwrite(file, sizeof(char), buf.st_size, f);
            
            if (file) free(file);
            fclose(f);
         } else {
            linkName = malloc(sizeof(char) * 4096);
            tmp = jrb_find_int(inode, buf.st_ino);
            strcpy(linkName, tmp->val.s);
            file = malloc(buf.st_size);
            fread(file, sizeof(char), buf.st_size, stdin);
            link(linkName, fileName);
            if (linkName) free(linkName);
            if (file) free(file);
         }

         chmod(fileName, buf.st_mode);
         time.actime = buf.st_atime;
         time.modtime = buf.st_mtime;
         utime(fileName, &time);
      }
   }

   jrb_rtraverse(tmp, dirs) {
      bufp = (struct stat *) tmp->val.s;
      chmod(tmp->key.s, bufp->st_mode);
      time.actime = bufp->st_atime;
      time.modtime = bufp->st_mtime;
      utime(tmp->key.s, &time);
   }

   if (fileName) free(fileName);
   if (linkName) free(linkName);
   jrb_traverse(tmp, dirs) if (dirs->key.s) free(tmp->key.s);
   jrb_free_tree (inode);
   jrb_free_tree (dirs);
   printf("Done reading tarfile\n");
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
      if (S_ISDIR(buf.st_mode)) {

         dir = opendir(s);
         if (dir == NULL) {
            perror("prsize");
            exit(1);
         }

         path = (char *) malloc((strlen(s) + 258) * sizeof(char));

         // iterate through the contents of the directory and add them to the dllist
         for (de = readdir(dir); de != NULL; de = readdir(dir)) {

            sprintf(path, "%s/%s", s, de->d_name);
            rPath = strdup(path);
            exists = lstat(rPath, &buf);

            if (exists < 0) {
               fprintf(stderr, "Couldn't stat %s\n", rPath);
               exit(1);
            } else if ((strcmp(de->d_name, ".") != 0) && (strcmp(de->d_name, "..") != 0)) {
               // ignore . and ..

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
