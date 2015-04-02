#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/inotify.h>
#include <limits.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>

 
#define MAX_EVENTS 1024 /*Max. number of events to process at one go*/
#define LEN_NAME 64 /*Assuming that the length of the filename won't exceed 64 bytes*/
#define EVENT_SIZE  ( sizeof (struct inotify_event) ) /*size of one event*/
#define BUF_LEN     ( MAX_EVENTS * ( EVENT_SIZE + LEN_NAME )) /*buffer to store the data of events*/
 

char *build_http_message(char *host, char *page,char *data) {
  char *query;
  char *tpl = "POST /%s HTTP/1.1\r\nHost: %s\r\nConnection: keep-alive\r\nContent-Length: %d\r\nContent-Type: application/x-www-form-urlencoded; charset=UTF-8\r\n\r\n%s\r\n";
  query = (char *)malloc(strlen(page)+strlen(host)+strlen(tpl));
  sprintf(query, tpl, page, host,strlen(data),data);
  return query;
}
int main( int argc, char **argv ){
  int length, i = 0, wd;
  int fd;
  char buffer[BUF_LEN];
  int sock;

  char * url="http://operation01:8080/hotitem/businesslog";
  char host[1000];
  char page[1000];
  int port;
  sscanf(url, "http://%99[^:]:%99d/%99[^\n]", host, &port, page);
  printf("host:%s\n",host);
  printf("port:%d\n",port);
  printf("page:%s\n",page);
  sock = create_socket_and_connect(url,host,port);

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
                    char *data = (char *)malloc(strlen("log=")+strlen(line));
                    sprintf(data, "log=%s", line);
                    char *http_message = build_http_message(host, page,data);
                    send(sock, http_message, strlen(http_message), 0);
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
  close(sock);
  return 0;
}
int create_tcp_socket() {
  int sock;
  if((sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0){
    perror("Can't create TCP socket");
    exit(1);
  }
  return sock;
}
char *get_ip(char *host) {
  struct hostent *hent;
  int iplen = 15; //XXX.XXX.XXX.XXX
  char *ip = (char *)malloc(iplen+1);
  memset(ip, 0, iplen+1);
  if((hent = gethostbyname(host)) == NULL) {
    herror("Can't get IP");
    exit(1);
  }
  if(inet_ntop(AF_INET, (void *)hent->h_addr_list[0], ip, iplen) == NULL) {
    perror("Can't resolve host");
    exit(1);
  }
  return ip;
}
int create_socket_and_connect(char* url,char* host,int port){
  struct sockaddr_in *remote;
  int sock;
  int tmpres;
  char *ip;
  char *http_message;
  char buf[BUFSIZ+1];



  sock = create_tcp_socket();
  ip = get_ip(host);
  printf("ip: %s\n", ip);
  remote = (struct sockaddr_in *)malloc(sizeof(struct sockaddr_in *));
  remote->sin_family = AF_INET;
  tmpres = inet_pton(AF_INET, ip, (void *)(&(remote->sin_addr.s_addr)));
  if( tmpres < 0)  {
    perror("Can't set remote->sin_addr.s_addr");
    exit(1);
  }else if(tmpres == 0) {
    fprintf(stderr, "%s is not a valid IP address\n", ip);
    exit(1);
  }
  remote->sin_port = htons(port);

  if(connect(sock, (struct sockaddr *)remote, sizeof(struct sockaddr)) < 0){
    perror("Could not connect");
    exit(1);
  }
  return sock;
}

