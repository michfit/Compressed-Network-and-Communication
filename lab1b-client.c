//NAME: Michelle Su
//EMAIL: xuehuasu@gmail.com
//ID: 404804135
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <string.h> 
#include <signal.h>
#include <getopt.h>
#include <poll.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <assert.h>
#include "zlib.h"


#define TIMEOUT 0
#define BUF_SIZE 256

char newCR[2] = {'\r','\n'}; 
struct termios newAttr, savedAttr;
char* log_filename;
int logfd;
int sockfd;
int logflag = 0;
int cprflag = 0;




  char tmpBuf[BUF_SIZE];
  int buf_i =0;
    // placeholder for the compressed (deflated) version of "a"
char b[BUF_SIZE];

    // placeholder for the UNcompressed (inflated) version of "b"
char c[BUF_SIZE];

int  deflate_proc (char *buf) {
    z_stream defstream;
    defstream.zalloc = Z_NULL;
    defstream.zfree = Z_NULL;
    defstream.opaque = Z_NULL;
    char* a;

    a = buf;
    // setup "a" as the input and "b" as the compressed output
//    printf("Uncompressed size is: %lu\n", strlen(a));
//    printf("Uncompressed string is: %s\n", a);

    defstream.avail_in = (uInt)strlen(a)+1; // size of input, string + terminator
    defstream.next_in = (Bytef *)a; // input char array
    defstream.avail_out = (uInt)sizeof(b); // size of output
    defstream.next_out = (Bytef *)b; // output char array

    // the actual compression work.
    deflateInit(&defstream, Z_BEST_COMPRESSION);
    deflate(&defstream, Z_FINISH);
    deflateEnd(&defstream);
    return  (int) defstream.total_out;
//    printf("Deflated size is: %d\n", (char*)defstream.next_out - b);
//    printf("Compressed string is: %s\n", b);

  }
void inflate_proc (char* buf) {
   // inflate b into c
    // zlib struct
    z_stream infstream;
    z_stream defstream;
    infstream.zalloc = Z_NULL;
    infstream.zfree = Z_NULL;
    infstream.opaque = Z_NULL;
    char *e;

    e = buf;


//    printf("Deflated size is: %lu\n", (char*)defstream.next_out - b);
//    printf("Compressed string is: %s\n", b);
    // setup "b" as the input and "c" as the compressed output
    infstream.avail_in = (uInt)((char*)defstream.next_out - e); // size of input
    infstream.next_in = (Bytef *)e; // input char array
    infstream.avail_out = (uInt)sizeof(c); // size of output
    infstream.next_out = (Bytef *)c; // output char array

    // the actual DE-compression work.
    inflateInit(&infstream);
    inflate(&infstream, Z_NO_FLUSH);
    inflateEnd(&infstream);
 //   printf("inflate size is: %lu\n", strlen(c));
 //   printf("inflate  string is: %s\n", c);
}
void restore_terminate_attr(){
  tcsetattr(STDIN_FILENO,TCSANOW,&savedAttr);
  if (logflag)
    close (logfd);
  close (sockfd);
}

/*
void sig_handler (int sigNum) {
  if(sflag && sigNum == SIGINT) {
     kill(pid, SIGINT);
   }
   if (sigNum == SIGPIPE) {
     exit (0);
   }
}
*/

void n_read_write (){
  char buf[BUF_SIZE];
  int bytes;

  bytes = read (sockfd, buf, BUF_SIZE);
  if (!(bytes >0)) {
      fprintf(stderr, "read bytes from socket is negative\n"); 
      restore_terminate_attr();
      exit (0);
  }
  if (logflag ) {
         dprintf(logfd,"RECEIVED %d bytes:  ", bytes);
         write(logfd,buf,bytes);
         dprintf(logfd,"\n");
  }
  if (cprflag) {
      inflate_proc (buf);
      strcpy (buf, c);
      bytes = (int) strlen(buf);
  }
   int i;
  for( i=0; i<bytes; i++) {
    if (buf[i] == 0x04){
      fprintf(stderr, "Receive EOF from socket\n"); 
      restore_terminate_attr();
      exit (0);
   }
  if ((buf[i] == '\n') || (buf[i] == '\r')) {
    write(STDOUT_FILENO,&newCR, 2*sizeof(char)); 
  }   
  else {
    write(STDOUT_FILENO,buf+i,sizeof(char)); 
   }
  }
}

void s_read_write (){
  char buf;
  
  char log_buf[14] ="SENT 1 bytes: ";
  char lf ='\n'; 
  int bytes;

  bytes = read (STDIN_FILENO, &buf, sizeof(char));
  if (!(bytes >0)) {
      fprintf(stderr, "read bytes from stdin is negative\n"); 
      restore_terminate_attr();
      exit (0);
  }
      if ((buf == '\n') || (buf == '\r')) {
	       write(STDOUT_FILENO,&newCR,2*sizeof(char)); 
	       buf= lf;
//	       write(sockfd,&lf,sizeof(char));
      }
      else {
	       write(STDOUT_FILENO,&buf,sizeof(char)); 
      } 
    //  if (!cprflag) {
	       write(sockfd,&buf,sizeof(char)); 
         if (logflag) {
            write(logfd,log_buf,sizeof(log_buf)); 
            write(logfd,&buf,sizeof(char)); 
            write(logfd,"\n", sizeof(char)); 
         }
    //   }
      /*
      else {
         bytes = deflate_proc(&buf);
	       write(sockfd,c,bytes); 
         if (logflag) {
            write(logfd,log_buf,sizeof(log_buf)); 
            dprintf(logfd,"SENT %d bytes:  ", bytes);
            write(logfd,c,bytes); 
            write(logfd,"\n", sizeof(char)); 
         }
     }
     */
  }

int poll_read_write() {
  struct pollfd fds[2];
  int ret;
  int bytes;

  char buf ='\0'; 
  /* watch stdin for input */
  fds[0].fd = STDIN_FILENO;
  fds[0].events = POLLIN | POLLHUP | POLLERR;

  /* watch cpp for input */
  fds[1].fd = sockfd;
  fds[1].events = POLLIN | POLLHUP | POLLERR;
   memset(tmpBuf, '\0', BUF_SIZE);

  while (1) {
    ret = poll(fds, 2, TIMEOUT * 1000);


    if (ret == -1) {
      perror ("poll");
      exit (1);
    }  
    if (fds[0].revents & POLLIN)
      {
      if (!cprflag)
	       s_read_write();
      else {
          bytes = read (STDIN_FILENO, &buf, sizeof (char)); 
//          fprintf(stderr, "in the chuck buf %c\n", buf);
 //         fprintf(stderr, "tmpBuf %s \n", tmpBuf);
          
         if ((buf == '\n') || (buf == '\r')) {
               write(STDOUT_FILENO,&newCR,2*sizeof(char));
               buf= '\n';
               tmpBuf[buf_i] = buf; 
               buf_i++;
    //           fprintf(stderr, "befoe compress tmpBuf  %s\n", tmpBuf);
               bytes = deflate_proc(tmpBuf);
               write(sockfd,b,bytes);
               if (logflag) {
                 dprintf(logfd,"SENT %d bytes:  ", bytes);
                 write(logfd,b, bytes);
                 write(logfd,"\n", sizeof(char));
               }
               memset(tmpBuf, '\0', BUF_SIZE);
               buf_i = 0;
          }
        else {
               write(STDOUT_FILENO,&buf,sizeof(char));
               tmpBuf[buf_i] = buf;
               buf_i ++;
        }
   }
}
    if (fds[1].revents & POLLIN) {
      n_read_write();
    }
    if (fds[0].revents & (POLLHUP | POLLERR)) {
	    fprintf(stderr, "stdin need to exit \n");
      restore_terminate_attr();
      exit (0);
    }
    if (fds[1].revents & (POLLHUP | POLLERR)) {
	    fprintf(stderr, "socket need to exit \n");
      restore_terminate_attr();
      exit (0);
    }
  }
}

int main(int argc,char** argv)
{

  struct option longopts[] = {
    {"port", required_argument, NULL, 'p'},
    {"log", required_argument, NULL, 'l'},
    {"hostname", required_argument, NULL, 'h'},
    {"compress", no_argument, NULL, 'c'},
    {0,0,0,0}
  }; 
  int opt, portno;
  int pflag =0;
  //int lflag =0;
  char *hostname = "localhost";
   
   
  //while there are still option characters
  while ((opt = getopt_long(argc, argv, "s:l:h:c", longopts, NULL)) != -1){
    switch(opt) {
    case 'p':
      portno = atoi(optarg);
      pflag = 1 ;
      break;
    case 'l':
      log_filename = optarg;
      logfd = creat(log_filename, S_IRWXU);
      logflag =1;
      if (logfd < 1){
	    fprintf(stderr, "can't open file %s\n", log_filename);
	    exit(1);
      }
      //lflag = 1;
      break;
    case 'h': 
      hostname = optarg;
      break;
    case 'c': 
      cprflag = 1;
      break;
    default: {
      fprintf(stderr, "unrecognized argument");   
      exit(1);
    }

    } 
  }
  //check if stdin is terminal
  if(!isatty(STDIN_FILENO)){
    perror ("STDIN is not a termial");
    exit (1);
  }
  // save and set attr
  if(tcgetattr(STDIN_FILENO, &savedAttr) < 0) {
    perror("stdin");
    exit (1);
  }

  if(tcgetattr(STDIN_FILENO, &newAttr) < 0) {
    perror("stdin");
    exit (1);
  }
  atexit(restore_terminate_attr);
  newAttr.c_iflag=ISTRIP;     // only lower 7 bit
  newAttr.c_oflag=0;          // no processing
  newAttr.c_lflag=0;
  newAttr.c_lflag &= ~(ICANON|ECHO); // clear ICANON and ECHO
  newAttr.c_cc[VMIN] = 1;
  newAttr.c_cc[VTIME] = 0;

  tcsetattr(STDIN_FILENO,TCSANOW,&newAttr);

  //socket stuff begins

  struct sockaddr_in serv_addr;
  struct hostent *server;

  //  char buf[256];


  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0){
    fprintf(stderr, "failed to open socket");
  }
  server = gethostbyname(hostname);
  if (server == NULL) {
    fprintf(stderr, "host doesn't exist\n");
    exit(1);
  }


  if (!pflag){
    fprintf(stderr, "port not specified");
    exit(1);
  }
  

  memset((char*) &serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  //   memcpy((char*) &serv_addr.sin_addr.s_addr, (char*) server->h_addr, server->h_length);
  memcpy((char *)server->h_addr, 
         (char *)&serv_addr.sin_addr.s_addr,
         server->h_length); 
  serv_addr.sin_port = htons(portno);

  if (connect(sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) < 0){
    fprintf(stderr, "couldn't connect to server\n");
    exit(1);
  }

  poll_read_write();
  restore_terminate_attr();
  return 0;
}
