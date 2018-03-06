// Matt Matto
// Lab 5 -- jtar.c
//
// COMPILE: gcc -I/home/plank/cs360/include -c jtar.c
//          gcc -I/home/plank/cs360/include -o jtar jtar.o /home/plank/cs360/objs/libfdr.a
//
// USAGE: ./jtar c [files ...] > tarfile <--- creates a tarfile containing [files ...]
//        ./jtar x < tarfile             <--- extracts the files from the tarfile
//
// DESCRIPTION:   takes a list of files and with the c command tars them up into a simple tarfile
//                then, with the x command, extracts them back into files and 
//                   sets all of their stats and properties back to the way they were

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
   char good, print;       // bools for if my command line is good input and whether I print debugging statements

   // make sure there are enough arguments
   if (argc < 2) {
      fprintf(stderr, "usage: jtar [cxv] [files ...]\n");
      exit(1);
   } 

   // make sure the second argument is a valid command
   good = 0;
   if ((strcmp(argv[1], "c") == 0) || (strcmp(argv[1], "x") == 0) || (strcmp(argv[1], "cv") == 0) || (strcmp(argv[1], "xv") == 0)) {
      good = 1;
   }
   if (good != 1) {
      fprintf(stderr, "usage: jtar [cxv] [files ...]\n");
      exit(1);
   }

   // sets the bool for if to print debug lines to stderr
   print = 0;
   if ((strcmp(argv[1], "cv") == 0) || (strcmp(argv[1], "xv") == 0)) print = 1;

   // checks the command line and calls the appropriate function
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
   int i, exists;                         // for loop iterator and stat() return value
   Dllist tmp, contents, p;               // tmp is a dllist iterator, contents is the contents of the files from the command line, and p keeps track of files processed
   JRB t, dirs, files, inode, hardLinks;  // JRBs that hold the directories, files, inodes, and hardlinks, respectively
   char *fileName;                        // used to hold the path to a file
   char *file;                            // used as a buffer for reading and writing the contents of a file
   struct stat buf, *bufp;                // stat buffer and buffer pointer
   FILE *f;                               // FILE to read and write from

   // make sure you don't call jtar on any previous directories, dangerous
   if (strncmp(argv[2], "..", 2) == 0) {
      fprintf(stderr, "usage: jtar [cxv] [files ...]\n");
      exit(1);
   }

   // allocate contents and proccessed dllists
   contents = new_dllist();
   p = new_dllist();

   // call process_files() on all the command line files 
   // when I finish this loop contents and p will be the same
   // they will hold the contents of the files from the command line
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

   // free p as I don't need it anymore
   if (p) free_dllist(p);

   // initialize all the JRBs
   t = make_jrb(); dirs = make_jrb(); files = make_jrb(); inode = make_jrb(); hardLinks = make_jrb();

   // traverse the contents dllist and push all the information into jrb's
   dll_traverse(tmp, contents) {
      fileName = tmp->val.s;

      exists = lstat(fileName, &buf);

      if (exists < 0) {
         fprintf(stderr, "File does not exist\n");
      } else {
         // file exists; check if directory or regular file
         
         if (S_ISDIR(buf.st_mode)) {
            //is a directory insert into directories dllist
            jrb_insert_str(dirs, fileName, new_jval_v(buf));
         } else if (S_ISREG(buf.st_mode)) {
            // is a file
            
            if (jrb_find_int(inode, buf.st_ino) == NULL) {
               
               jrb_insert_int(inode, buf.st_ino, new_jval_i(buf.st_ino));
               jrb_insert_str(files, fileName, new_jval_s(fileName));
            } else {
               
               // is a hardlink
               jrb_insert_str(hardLinks, fileName, new_jval_s(tmp->val.s));
            }
         
         }
      }
   } // end of pushing into jrb's

   // traverse jrbs and print contents to stdout starting with dirs, then files and hardlinks
   // all prints consist of printing the length of the fileName, for using fscanf in extractTar()
   // printing the actual fileName
   // and printing the stat buf info
   // if printing a regular file, then I also print the contents of the file following the stat buf info
   jrb_traverse(t, dirs) {
      fileName = t->key.s;
      lstat(fileName, &buf);
      printf("%d", strlen(fileName));
      fwrite(fileName, sizeof(char), strlen(fileName), stdout);
      fwrite(&buf, sizeof(struct stat), 1, stdout);
   } // end of print dirs
   
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
   } // end of print files

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
   } // end of print hardlinks
} // end of createTar

// reads in the contents of my tarfile on stdin and extracts the fileName and stat buf info
// then it check whether it is a directory or a file
// if it is a directory, creates the directory and fills in its stat info with and stat buf info read in on stdin
// if it is a regular file, it then checks if it is already in the inodes jrb
//    If it is not, I know it is a regular file and add it to the inode jrb
//    I then create the file, and write the contents of the file
// else it must be a hardlink
//    go search the inode jrb for the file the hardlink is linked to
//    link the two files
// then whether it was a regular file or a hardlink, set the permissions and times
// Next I traverse the dirs and set their permissions and times from the inside out
// and now free everything
int extractTar(int argc, char **argv, char print) {
   JRB tmp, dirs, inode;      // JRBs for holding the dirs and inodes
   char *fileName;            // string for holding the file's path
   char *file;                // buffer for reading and writing the contents of a file
   char *linkName;            // string for holding the fileName to link in a hardlink
   int fileNameLen;           // length of a fileName to use with fscanf
   struct stat buf, *bufp;    // stat buffers
   FILE *f;                   // FILE pointer to open and read files
   struct utimbuf time;       // time struct for setting file time porperties

   // initialize the JRBs
   dirs = make_jrb(); inode = make_jrb();

   // reads in the contents of my tarfile on stdin and extracts the fileName and stat buf info
   while (fscanf(stdin, "%d", &fileNameLen)) {
      memset(&buf, 0, sizeof(struct stat));
      fileName = calloc(fileNameLen+1, sizeof(char));
      fread(fileName, sizeof(char), fileNameLen, stdin);
      if (strcmp(fileName, "") == 0) break;
      
      fread(&buf, sizeof(struct stat), 1, stdin);
      
      // then it check whether it is a directory or a file
      // if it is a directory, creates the directory and fills in its stat info with and stat buf info read in on stdin
      if (S_ISDIR(buf.st_mode)) {
         
         mkdir(fileName, 0777);
         bufp = malloc(sizeof(struct stat));
         memcpy(bufp, &buf, sizeof(struct stat));
         jrb_insert_str(dirs, strdup(fileName), new_jval_v(bufp));
      
      } else if (S_ISREG(buf.st_mode)) {
         // if it is a regular file, it then checks if it is already in the inodes jrb
         
         // If it is not, I know it is a regular file and add it to the inode jrb
         if (jrb_find_int(inode, buf.st_ino) == NULL) {
            jrb_insert_int(inode, buf.st_ino, new_jval_s(fileName));

            // I then create the file, and write the contents of the file
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
            // else it must be a hardlink
            
            // go search the inode jrb for the file the hardlink is linked to
            linkName = malloc(sizeof(char) * 4096);
            tmp = jrb_find_int(inode, buf.st_ino);
            strcpy(linkName, tmp->val.s);
            file = malloc(buf.st_size);
            fread(file, sizeof(char), buf.st_size, stdin);
            
            //    link the two files
            link(linkName, fileName);
            if (linkName) free(linkName);
            if (file) free(file);
         }

         // then whether it was a regular file or a hardlink, set the permissions and times
         chmod(fileName, buf.st_mode);
         time.actime = buf.st_atime;
         time.modtime = buf.st_mtime;
         utime(fileName, &time);
      }
   }

   // Next I traverse the dirs and set their permissions and times from the inside out
   jrb_rtraverse(tmp, dirs) {
      bufp = (struct stat *) tmp->val.s;
      chmod(tmp->key.s, bufp->st_mode);
      time.actime = bufp->st_atime;
      time.modtime = bufp->st_mtime;
      utime(tmp->key.s, &time);
   }

   // and now free everything
   if (fileName) free(fileName);
   jrb_traverse(tmp, dirs) if (dirs->key.s) free(tmp->key.s);
   jrb_free_tree (inode);
   jrb_free_tree (dirs);
}

// read the file given by s and make sure it exists
// if it is a directory open it and push the contents into a dllist
// now recursively call preocess_files() on the contents of the dllist
// when process_files() finally returns, both d and p will have the directory and contents of the directory in them
int process_files(char *s, Dllist d, Dllist p, char print) {
   DIR *dir;            // directory pointer for opening and traversing directories
   struct stat buf;     // stat buffer
   struct dirent *de;   // used for iterating through a directory and reading the names of the contents
   int exists;          // bool for the return value of lstat()
   char *path;          // string for the path to the file
   char *rPath;         // string to hole the full path created by line 349
   char p1;             // bool for checking if rPath is already in the contents dllist
   Dllist tmp, ptmp;    // dllist iterators
   char *name;          // used to help call process_files()

   // my fuck up insurance
   if (strcmp(s + strlen(s) - 6, "jtar.c") == 0) {
      fprintf(stderr, "Don't call jtar on a directory connected to jtar.c ya biscuit-head.\n");
      exit(1);
   }

   exists = lstat(s, &buf);

   // check if a given file, s, exists
   if (exists < 0) {
      printf("%s does not exist\n", s);
      return -1;
   } else {
      
      // if it does, then checks whether it is a directory or a regular file
      if (S_ISDIR(buf.st_mode)) {

         // open the directory
         dir = opendir(s);
         if (dir == NULL) {
            perror("prsize");
            exit(1);
         }

         path = (char *) malloc((strlen(s) + 258) * sizeof(char));

         // iterate through the contents of the directory and add them to the dllist
         for (de = readdir(dir); de != NULL; de = readdir(dir)) {

            // build the path for the file within the directory
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
               // the only things that should get here are . and ..
            }
         }

         // close the directory
         closedir(dir);

         // traverse the dllist and recursively call process_files() on everything
         dll_traverse(tmp, d) {
            p1 = 0;
            dll_traverse(ptmp, p) {
               if (strcmp(ptmp->val.s, tmp->val.s) == 0) p1 = 1;
            }
            if ((tmp != NULL) && (p1 == 0)) {
               name = strdup(tmp->val.s);

               // whenever I call process_files on something, I add it to the p dllist to 'mark' it as processed
               // there is probably a much better way to do this
               dll_append(p, new_jval_s(name));

               process_files(name, d, p, print);
            }
         }
      } else if (S_ISREG(buf.st_mode)) {
         // regular files are untouched
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
