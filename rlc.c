#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/inotify.h>
#include <limits.h>
 
#define MAX_EVENTS 1024 /*Max. number of events to process at one go*/
#define LEN_NAME 64 /*Assuming that the length of the filename won't exceed 64 bytes*/
#define EVENT_SIZE  ( sizeof (struct inotify_event) ) /*size of one event*/
#define BUF_LEN     ( MAX_EVENTS * ( EVENT_SIZE + LEN_NAME )) /*buffer to store the data of events*/
 

int main( int argc, char **argv ){
  int length, i = 0, wd;
  int fd;
  char buffer[BUF_LEN];

  FILE * fp;
  char * line = NULL;
  size_t len = 0;
  ssize_t readlen;



  /* Initialize Inotify*/
  fd = inotify_init();
  if ( fd < 0 ) {
    perror( "Couldn't initialize inotify");
  }
 
  /* add watch to starting directory */
  wd = inotify_add_watch(fd, argv[1], IN_CREATE | IN_MODIFY | IN_DELETE); 
 
  if (wd == -1)
    {
      printf("Couldn't add watch to %s\n",argv[1]);
    }
  else
    {
      printf("Watching:: %s\n",argv[1]);
    }
 
  char * name = malloc(strlen(argv[1])+strlen(argv[2]) + 2);
  sprintf(name,"%s/%s", argv[1], argv[2]);
  printf("open : %s\n",name);
  fp = fopen(name, "r");
  while ((readlen = getline(&line, &len, fp)) != -1) {
                    readlen = getline(&line, &len, fp);
                    printf("%s", line);
    
  }

  /* do it forever*/
  while(1)
    {
      i = 0;
      length = read( fd, buffer, BUF_LEN );  
 
      if ( length < 0 ) {
        perror( "read" );
      }  
 
      while ( i < length ) {
        struct inotify_event *event = ( struct inotify_event * ) &buffer[ i ];
        if ( event->len ) {
          if ( event->mask & IN_CREATE) {
            if (event->mask & IN_ISDIR){} else{
                printf("created %s\n",event->name);
                if(strcmp(event->name , argv[2])==0){
                    if (fp != NULL){
                        fclose(fp);
                    }
                    //fp = fopen(argv[2], "r");
                    fp = fopen(name, "r");
                }
            }
          }
           
          if ( event->mask & IN_MODIFY) {
            if (event->mask & IN_ISDIR){}
            else{
              if(strcmp(event->name , argv[2])==0){
                while ((readlen = getline(&line, &len, fp)) != -1) {
                    readlen = getline(&line, &len, fp);
                    printf("%s", line);
                }
              }
            }
          }
           
          if ( event->mask & IN_DELETE) {
            if (event->mask & IN_ISDIR){} else{}
          }  
 
 
          i += EVENT_SIZE + event->len;
        }
      }
    }
 
  /* Clean up*/
  inotify_rm_watch( fd, wd );
  close( fd );
   
  return 0;
}
