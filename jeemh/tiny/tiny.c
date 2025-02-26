#include "csapp.h"
void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize, char *method);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs, char *method);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg,
                 char *longmsg);
int main(int argc, char **argv) {
  int listenfd, connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;
  /* Check command line args */
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }
  listenfd = Open_listenfd(argv[1]);
  while (1) {
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr,&clientlen);  // line:netp:tiny:accept
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE,0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);
    doit(connfd);   // line:netp:tiny:doit
    Close(connfd);  // line:netp:tiny:close
  }
}
void doit(int fd){
  int is_static;
  struct stat sbuf;
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char filename[MAXLINE], cgiargs[MAXLINE];
  rio_t rio;
  Rio_readinitb(&rio,fd);
  if(!Rio_readlineb(&rio,buf,MAXLINE))
    return;
  printf("Request headers:\n");
  printf("%s",buf);
  sscanf(buf, "%s %s %s",method,uri,version);
  if(strcasecmp(method, "GET") && strcasecmp(method, "HEAD")){
    clienterror(fd,method,"501","Not Implemented","Tiny does not implement this method");
    return;
  }
  read_requesthdrs(&rio);
  is_static = parse_uri(uri,filename,cgiargs);
  if(stat(filename,&sbuf) < 0){
    clienterror(fd,filename,"404","Not found","Tiny couldn't find this file");
    return;
  }
  if(is_static){
    if(!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)){
      clienterror(fd,filename,"403","Forbidden","Tiny couldn't read the file");
      return;
    }
    serve_static(fd,filename,sbuf.st_size,method);
  }else{
    if(!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)){
      clienterror(fd,filename,"403","Forbidden", "Tiny couldn't run the CGI program");
      return;
    }
    serve_dynamic(fd,filename,cgiargs, method);
  }
}
void read_requesthdrs(rio_t *rp){
  char buf[MAXLINE];
  Rio_readlineb(rp,buf,MAXLINE);
  printf("%s", buf);
  while(strcmp(buf,"\r\n")){
    Rio_readlineb(rp,buf,MAXLINE);
    printf("%s",buf);
  }
  return;
}
int parse_uri(char *uri, char *filename, char *cgiargs){
  char *ptr;
  if(!strstr(uri,"cgi-bin")){
    strcpy(cgiargs,"");
    strcpy(filename,".");
    strcat(filename,uri);
    if(uri[strlen(uri)-1] == '/')
      strcat(filename,"home.html");
    return 1;
  }else{
    ptr = index(uri, '?');
    if(ptr){
      strcpy(cgiargs,ptr+1);
      *ptr = '\0';
    }else{
      strcpy(cgiargs,"");
    }
    strcpy(filename,".");
    strcat(filename,uri);
    return 0;
  }
}
void get_filetype(char *filename,char *filetype){
  if(strstr(filename,".html")){
    strcpy(filetype,"text/html");
  }else if(strstr(filename,".gif")){
    strcpy(filetype,"image/gif");
  }else if(strstr(filename,".png")){
    strcpy(filetype,"image/png");
  }else if(strstr(filename, ".jpg")){
    strcpy(filetype,"image/jpeg");
  }else{
    strcpy(filetype,"text/plain");
  }
}
void serve_static(int fd, char *filename, int filesize, char *method){
  int srcfd;
  char *srcp, filetype[MAXLINE], buf[MAXLINE];
  get_filetype(filename,filetype);
  sprintf(buf,"HTTP/1.1 200 OK\r\n");
  Rio_writen(fd,buf,strlen(buf));
  sprintf(buf,"Server: Tiny Web Server\r\n");
  Rio_writen(fd,buf,strlen(buf));
  sprintf(buf,"Content-length: %d\r\n", filesize);
  Rio_writen(fd,buf,strlen(buf));
  sprintf(buf,"Content-type: %s\r\n\r\n",filetype);
  Rio_writen(fd,buf,strlen(buf));

  if (strcasecmp(method, "HEAD") == 0)
    return;

  srcfd = Open(filename,O_RDONLY,0);
  srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
  Close(srcfd);
  Rio_writen(fd,srcp,filesize);
  Munmap(srcp,filesize);
  // 11.9 malloc
  // srcfd = Open(filename, O_RDONLY, 0);
  // srcp = (char *)malloc(sizeof(filesize));
  // Rio_readn(srcfd, srcp, filesize);
  // Close(srcfd);
  // Rio_writen(fd, srcp, filesize);
  // free(srcp);
}
void serve_dynamic(int fd, char *filename, char *cgiargs, char *method){
  char buf[MAXLINE], *emptylist[] = {NULL};
  sprintf(buf,"HTTP/1.1 200 OK\r\n");
  Rio_writen(fd,buf,strlen(buf));
  sprintf(buf,"Server: Tiny Web Server\r\n");
  Rio_writen(fd,buf,strlen(buf));

  if(Fork() == 0){
    setenv("QUERY_STRING", cgiargs, 1);
    setenv("REQUEST_METHOD", method, 1);
    Dup2(fd,STDOUT_FILENO);
    Execve(filename,emptylist,environ);
  }
  Wait(NULL);
}
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg){
  char buf[MAXLINE];
  sprintf(buf, "HTTP/1.1 %s %s\r\n", errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "content-type: text/html\r\n\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "<html><title>Tiny Error</title>");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "<body bgcolor=""ffffff"">\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "%s: %s\r\n",errnum,shortmsg);
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf,"<p>%s: %s\r\n",longmsg,cause);
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf,"<hr><em>The Tiny Web server</em>\r\n");
  Rio_writen(fd, buf, strlen(buf));
}