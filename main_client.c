/*
 * main_client.c
 *
 *  Created on: Apr 20, 2016
 *      Author: michele
 */


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>


// the port number we use
#define def_port_number 3117

void error(const char *msg)
{
    perror(msg);
    exit(0);
}

int main(int argc, char *argv[])
{
    int sockfd, portno, n;
    struct sockaddr_in serv_addr;
    struct hostent *server;

    char buffer[256];

    portno = def_port_number;
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        error("ERROR opening socket");
    server = gethostbyname("localhost");
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host\n");
        exit(0);
    }
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr,
         (char *)&serv_addr.sin_addr.s_addr,
         server->h_length);
    serv_addr.sin_port = htons(portno);
    if (connect(sockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0)
        error("ERROR connecting");
    {
    	srand(time(NULL));
    	unsigned int i;
    	unsigned int nloop = 1+ (rand()%64);

    	for (i = 0; i < nloop; i++)
    	{
    	    snprintf(buffer,sizeof(buffer), "hello how are you?");
    	    printf("Sending the message %i of %i: %s\n",i+1,nloop,buffer);
    	    n = write(sockfd,buffer,strlen(buffer));
    	    if (n <= 0)
    	    {
				error("ERROR writing to socket (socket closed?)\n");
    	    }
    	    bzero(buffer,256);
    	    n = read(sockfd,buffer,255);
    	    if (n <= 0)
    	    {
				error("ERROR reading from socket (socket closed?)\n");
    	    }
    	    printf("Message received: %s\n",buffer);
    	    if (i+1 < nloop)
    	    {
        	    // sleep random time
    	    	unsigned int us_sleep = 200000 + (rand()%16)*100000;
    	    	printf("sleeping for %i us\n",us_sleep);
        	    usleep(us_sleep);
    	    }
    	}
    }
    close(sockfd);
    return 0;
}
