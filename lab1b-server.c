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
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/wait.h>
#include <assert.h>
#include "zlib.h"

#define TIMEOUT 0
#define BUF_SIZE 256

int p2childP[2];
int c2parentP[2];
pid_t  pid=-1;
char newCR[2] = {'\r','\n'}; 
struct termios newAttr, savedAttr;
int sockfd, newsockfd;

int cprflag = 0;




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

void error(char *msg)
{
    perror(msg);
    exit(1);
}
 void exit_msg(){
    
        int status = 0;
        //  get the shell's exit status
        waitpid(0, &status, 0);
        //  lower 8 (actually 7) bits of the status is the signal, higher 8 bits of the status is the status
        fprintf(stderr, "SHELL EXIT SIGNAL=%d STATUS=%d\n", WTERMSIG(status), WEXITSTATUS(status));
        close (p2childP[1]);
        close (c2parentP[1]);
        close (p2childP[0]);
        close (c2parentP[0]);
        close (sockfd);
        close (newsockfd);
        exit (0);
}
void sig_handler (int sigNum) {
  if(sigNum == SIGINT) {
     kill(pid, SIGINT);
     exit_msg();
   }
   if (sigNum == SIGPIPE) {
     exit_msg();
    }
   if (sigNum == SIGTERM) {
     exit_msg();
   }
}

 void n_read_write (int fd){
   char buf[BUF_SIZE];
   int  bytes, i;
   int  cpr_len;


    bytes = read (fd, buf, BUF_SIZE);
    if (!(bytes>0)) {
       fprintf(stderr, "Read from Shell is %d\n", bytes);
        exit_msg ();
        exit (0);
    }
//     fprintf(stderr, "read data from the shell  %s\n", buf);
    for (i=0; i<bytes; i++) {
       if (buf[i] == 0x04){
           fprintf(stderr, "EOF received from the shell\n");
           exit_msg ();
           exit (0);
        }
    }
   if(!cprflag)
      {
        write(newsockfd,buf, strlen(buf));
//          fprintf(stderr, "read from shell %s %d\n", buf, (int)strlen(buf));
      }
   else {
          cpr_len = deflate_proc(buf);
          write(newsockfd,b, cpr_len);
      }
}
 void s_read_write (){
   char buf[BUF_SIZE];
   int bytes;
   char lf ='\n'; 
    
 //  printf ("p2childP[1]  %d\n", p2childP[1]);
   bytes = read (newsockfd, buf, BUF_SIZE); 
    if (!(bytes>0)) {
       fprintf(stderr, "Read from socket is %d\n", bytes);
        exit_msg ();
        exit (0);
    }
   if(cprflag) {
        inflate_proc(buf);
        strcpy (buf, c);
        bytes = (int) strlen(buf);
       fprintf(stderr, "Read from socket and after inflate is %d  %s\n", bytes, buf);
   }
    int i;
   for ( i=0; i<bytes; i++) {
          if (buf[i] == 0x04){
              close(p2childP[1]);
          }
          if (buf[i] == 0x03) { 
               kill(pid, SIGINT);
          }
          else if ((buf[i] == '\n') || (buf[i] == '\r')) {
               write(p2childP[1],&lf,sizeof(char)); 
//          printf ("writen to pipe  %s\n", &buf); 
          }
          else {
                write(p2childP[1],buf+i,sizeof(char)); 
//          printf ("write to pipe  %s\n", &buf); 
          } 
   }   
}
int poll_read_write() {
  struct pollfd fds[2];
	int ret;

	/* watch socketfd for input */
	fds[0].fd = newsockfd;
	fds[0].events = POLLIN | POLLHUP | POLLERR;

	/* watch c2parentP for input */
	fds[1].fd = c2parentP[0];
	fds[1].events = POLLIN | POLLHUP | POLLERR;
  
  while (1) {
	  ret = poll(fds, 2, TIMEOUT * 1000);

	  if (ret == -1) {
		   perror ("poll");
		   exit (1);
	  }  

	  if (fds[0].revents & POLLIN)
    {
       s_read_write ();
     }
	  if (fds[1].revents & POLLIN) {
       n_read_write (c2parentP[0]);
    }
	  if (fds[0].revents & (POLLHUP | POLLERR) ) {
	     close (p2childP[1]);
  }
	  if (fds[1].revents & (POLLHUP | POLLERR) ) {
	     exit_msg();
  }
 }
}

int main(int argc,char** argv)
{

   struct option longopts[] = {
     {"portno", required_argument, NULL, 'p'},
     {"compress", no_argument, NULL, 'c'},
     {0,0,0,0}
   }; 



	int opt;
	int portno;

	//while there are still option characters
	while ((opt = getopt_long(argc, argv, "p:c", longopts, NULL)) != -1){
		switch(opt) {
			case 'p':
			   portno = atoi (optarg) ;
			   break;
			case 'c':
			   cprflag = 1 ;
			   break;

      default: {
						perror("option character  not recognized");
						exit(1);
					}

		 } //end of switch 
	 } //end of while 

	 signal (SIGINT, sig_handler);
	 signal (SIGPIPE, sig_handler);

//     int sockfd, newsockfd, clilen;
     int clilen;
     struct sockaddr_in serv_addr, cli_addr;

     sockfd = socket(AF_INET, SOCK_STREAM, 0);
     if (sockfd < 0)
        error("ERROR opening socket");
     memset((char *) &serv_addr, 0, sizeof(serv_addr));
     serv_addr.sin_family = AF_INET;
     serv_addr.sin_addr.s_addr = INADDR_ANY;
     serv_addr.sin_port = htons(portno);
     if (bind(sockfd, (struct sockaddr *) &serv_addr,
              sizeof(serv_addr)) < 0)
              error("ERROR on binding");
     listen(sockfd,5);
     clilen = sizeof(struct sockaddr_in);
     newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, (socklen_t*)&clilen);
     if (newsockfd < 0)
           error("ERROR on accept");

      if (pipe(p2childP)==-1)
      {
             fprintf(stderr, "Pipe Failed" );
             exit(1);
       }
       if (pipe(c2parentP)==-1)
       {
          fprintf(stderr, "Pipe Failed" );
          exit(1);
       }
 
       pid = fork();
//       printf ("pid is %d\n", pid);
 
    if (pid < 0)
    {
        fprintf(stderr, "fork Failed" );
        return 1;
    }
 
    else if (pid == 0)
    {
         
        close(p2childP[1]);  // Close writing end of first pipe
        close(c2parentP[0]);  // Close reading end of second pipe
        // re-directed
        if (dup2(p2childP[0], STDIN_FILENO)< 0) {
          printf("dup2 error\n");
          exit (1);
        }
        if(dup2(c2parentP[1],STDOUT_FILENO)<0){
          perror ("dup2 error");
          printf("dup2 error\n");
          exit (1);
        } 
        if(dup2(c2parentP[1],STDERR_FILENO)<0){
          perror ("dup2 error");
          printf("dup2 error\n");
          exit (1);
        } 
        close (p2childP[0]);
        close (c2parentP[1]);
        // format arguments for shell
    	    char *execvp_argv[2];
    	    char execvp_filename[] = "/bin/bash";
    	    execvp_argv[0] = execvp_filename;
    	    execvp_argv[1] = NULL;

    	    // execute shell with it's filename as an argument
    	    if (execvp(execvp_filename, execvp_argv) == -1) {
        		fprintf(stderr, "error: error in executing shell");
        		exit(1);
        	}
     }
     else{
       // Parent process
        close(p2childP[0]);  // Close reading end of first pipe
        close(c2parentP[1]); // Close writing end of second pipe
        poll_read_write();
 
     }  //end else
     close (sockfd);
     close (newsockfd);
     exit(0);
 }
