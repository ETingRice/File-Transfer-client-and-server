#include "common.h"
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <iostream>
#include <string>
#include <thread>
using namespace std;

#ifndef _TCPRequestChannel_H_
#define TCPRequestChannel_H_

class TCPRequestChannel{
private:
     /* Since a TCP socket is full-duplex, we need only one.
     This is unlike FIFO that needed one read fd and another
      for write from each side  */
    int sockfd;
public:
     /* Constructor takes 2 arguments: hostname and port not
     If the host name is an empty string, set up the channel for
      the server side. If the name is non-empty, the constructor
     works for the client side. Both constructors prepare the
     sockfd  in the respective way so that it can works as a    server or client communication endpoint*/
   TCPRequestChannel (const string host_name, const string port_no){
      if(host_name == ""){ //server
         int new_fd;  // listen on sock_fd, new connection on new_fd
         struct addrinfo hints, *serv;
         struct sockaddr_storage their_addr; // connector's address information
         socklen_t sin_size;
         char s[INET6_ADDRSTRLEN];
         int rv;

         memset(&hints, 0, sizeof hints);
         hints.ai_family = AF_UNSPEC;
         hints.ai_socktype = SOCK_STREAM;
         hints.ai_flags = AI_PASSIVE; // use my IP

         if ((rv = getaddrinfo(NULL, const_cast<char*>(port_no.c_str()), &hints, &serv)) != 0) {
            cerr  << "getaddrinfo: " << gai_strerror(rv) << endl;
            exit(-1);
         }
         if ((sockfd = socket(serv->ai_family, serv->ai_socktype, serv->ai_protocol)) == -1) {
            perror("server: socket");
            exit(-1);
         }
         if (bind(sockfd, serv->ai_addr, serv->ai_addrlen) == -1) {
            close(sockfd);
            perror("server: bind");
            exit(-1);
         }
         freeaddrinfo(serv); // all done with this structure

         if (listen(sockfd, 20) == -1) {
            perror("listen");
            exit(1);
         }
         
         //cout << "server: waiting for connections..." << endl;
         
      }else{ //client
         struct addrinfo hints, *res;

         // first, load up address structs with getaddrinfo():
         memset(&hints, 0, sizeof hints);
         hints.ai_family = AF_UNSPEC;
         hints.ai_socktype = SOCK_STREAM;
         int status;
         //getaddrinfo("www.example.com", "3490", &hints, &res);
         if ((status = getaddrinfo (const_cast<char*>(host_name.c_str()), const_cast<char*>(port_no.c_str()), &hints, &res)) != 0) {
            cerr << "getaddrinfo: " << gai_strerror(status) << endl;
            exit(-1);
         }

         // make a socket:
         sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
         if (sockfd < 0){
            perror ("Cannot create socket");	
            exit(-1);
         }

         // connect!
         if (connect(sockfd, res->ai_addr, res->ai_addrlen)<0){
            perror ("Cannot Connect");
            exit(-1);
         }
         //
         //cout << "Connected " << endl;
         freeaddrinfo (res);
       
      }
   }
/* This is used by the server to create a channel out of a newly accepted client socket. Note that an accepted client socket is ready for communication */
   TCPRequestChannel (int a){
      sockfd = a;
   }
   /* destructor */
   ~TCPRequestChannel(){
      close(sockfd);
   }

   int cread (void* msgbuf, int buflen){
      return read (sockfd, msgbuf, buflen); 

   }

   int cwrite(void* msgbuf, int msglen){
      return write (sockfd, msgbuf, msglen);

   }

    /* this is for adding the socket to the epoll watch list */
   int getfd(){
      return sockfd;
   }
};

#endif