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

struct url{
    char *url;
    char schema[100];
    char host[1000];
    char page[1000];
    int port;
};

struct url parse_url(char *url);



/*
   build http1.1 post message
   */
char *build_http_message(char *host, char *page,char *data,char *query);
/*
   create socket connected to specified url
   */
int create_socket_and_connect(struct url url);
/*
   open file with read mode
   */
FILE * open_file(char *dir,char *file);
/*
   read file to the end
   */
void read_file_to_end(FILE *fp,struct url url,int *sock,void(*process)(char *,struct url url,int *sock));

void do_nothing(char *line,struct url url,int *sock);
void send_log_to_server(char *line,struct url url,int *sock);

void watch(char *dir,char *file,uint32_t mask,struct url url,int *sock);

/*
usage: ./rlc xx/xx/log xxx.log http://operation01:8080/hotitem/businesslog
*/
int main( int argc, char **argv ){
    struct url url = parse_url(argv[3]);
    int sock = create_socket_and_connect(url);
    watch(argv[1],argv[2],IN_CREATE | IN_MODIFY | IN_DELETE,url,&sock);
    close(sock);
    return 0;
}
void process_event(struct inotify_event *event,struct url url,int *sock,FILE **fp,char *dir,char *file){
    if ( event->mask & IN_CREATE) {
        if (event->mask & IN_ISDIR){} else{
            printf("created %s\n",event->name);
            if(strcmp(event->name , file)==0){
                fclose(*fp);
                *fp = open_file(dir, file);
            }
        }
    }

    if ( event->mask & IN_MODIFY) {
        if (event->mask & IN_ISDIR){}
        else{
            if(strcmp(event->name , file)==0){
                read_file_to_end(*fp,url,sock,send_log_to_server);
            }
        }
    }

    if ( event->mask & IN_DELETE) {
        if (event->mask & IN_ISDIR){} else{}
    }
}
void watch(char *dir,char *file,uint32_t mask,struct url url,int *sock){
    int length, i = 0, wd;
    int fd;
    char buffer[BUF_LEN];
    FILE *fp;
    /* Initialize Inotify*/
    fd = inotify_init();
    /* add watch to starting directory */
    wd = inotify_add_watch(fd, dir, mask);

    fp = open_file(dir,file);
    read_file_to_end(fp,url,sock,do_nothing);

    /* do it forever*/
    while(1)
    {
        int i = 0;
        int length = read( fd, buffer, BUF_LEN );
        while ( i < length ) {
            struct inotify_event *event = ( struct inotify_event * ) &buffer[ i ];
            if ( event->len ) {
                process_event(event,url,sock,&fp,dir,file);
                i += EVENT_SIZE + event->len;
            }
        }
    }

    /* Clean up*/
    inotify_rm_watch( fd, wd );
    close( fd );
    close(sock);
}

void do_nothing(char *line,struct url url,int *sock){
}
void send_log_to_server(char *line,struct url url,int *sock){
    char *data = (char *)malloc(strlen("log=")+strlen(line));
    sprintf(data, "log=%s", line);
    char query[100000];
    build_http_message(url.host, url.page,data,&query[0]);
    //send(sock, query, strlen(query), 0);
    int ret = send(*sock, query, strlen(query), MSG_NOSIGNAL);
    if (ret == -1) {
        //retry
        printf("retry connect!\n");
        *sock = create_socket_and_connect(url);
        send(*sock, query, strlen(query), 0);
    }
    free(data);
}
void read_file_to_end(FILE *fp,struct url url,int *sock,void(*process)(char *,struct url url,int *sock)){
    char * line = NULL;
    size_t len = 0;
    ssize_t readlen;
    while ((readlen = getline(&line, &len, fp)) != -1) {
        readlen = getline(&line, &len, fp);
        printf("%s", line);
        (*process)(line,url,sock);
    }
}

FILE * open_file(char *dir,char *file){
    char * name = malloc(strlen(dir)+strlen(file) + 2);
    sprintf(name,"%s/%s", dir, file);
    printf("open : %s\n",name);
    FILE *fp = fopen(name, "r");
    free(name);
    return fp;
}
struct url parse_url(char *url){
    struct url u;
    sscanf(url, "%[^://]://%99[^:]:%99d/%99[^\n]",u.schema, u.host, &u.port, u.page);
    printf("host:%s\n",u.host);
    printf("port:%d\n",u.port);
    printf("page:%s\n",u.page);
    return u;
}
char *build_http_message(char *host, char *page,char *data,char *query) {
    //char *query;
    char *tpl = "POST /%s HTTP/1.1\r\nHost: %s\r\nConnection: keep-alive\r\nContent-Length: %d\r\nContent-Type: application/x-www-form-urlencoded; charset=UTF-8\r\n\r\n%s\r\n\0";
    //query = (char *)malloc(strlen(page)+strlen(host)+strlen(tpl));
    sprintf(query, tpl, page, host,strlen(data),data);
    return query;
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
int create_socket_and_connect(struct url url){
    struct sockaddr_in *remote;
    int sock;
    int tmpres;
    char *ip;
    char *http_message;
    char buf[BUFSIZ+1];



    sock = create_tcp_socket();
    ip = get_ip(url.host);
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
    remote->sin_port = htons(url.port);

    if(connect(sock, (struct sockaddr *)remote, sizeof(struct sockaddr)) < 0){
        perror("Could not connect");
        exit(1);
    }
    return sock;
}

