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
#include <arpa/inet.h>
#include "ASAC_ZigBee_network_commands.h"
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

    char buffer[sizeof(type_ASAC_Zigbee_interface_command)];

    portno = def_port_number;
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        error("ERROR opening socket");
#define def_use_local_host
#ifdef def_use_local_host
    struct hostent *server;
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
#else
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(portno);
    inet_aton("10.0.0.15", &serv_addr.sin_addr);
#endif
    if (connect(sockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0)
        error("ERROR connecting");
    {
    	srand(time(NULL));
    	unsigned int i;
    	unsigned int nloop = 1+ (rand()%64);

    	for (i = 0; i < nloop; i++)
    	{
    		type_ASAC_Zigbee_interface_command *p_command =(type_ASAC_Zigbee_interface_command *) buffer;
    		// write the command code
    		p_command->code = enum_ASAC_ZigBee_interface_command_outside_send_message;
    		// write the command body
    	    int n_chars = snprintf((char*)p_command->req.outside_send_message.message,sizeof(p_command->req.outside_send_message.message), "(%i / %i) hello how are you?", (i+1), nloop);
    	    printf("Sending the message %i of %i: %s\n",i+1,nloop,p_command->req.outside_send_message.message);

    	    // the number of bytes written
    	    int n_bytes_in_buffer = n_chars + def_size_ASAC_Zigbee_interface_code(p_command);
    	    n = write(sockfd, buffer, n_bytes_in_buffer);
    	    if (n <= 0)
    	    {
				error("ERROR writing to socket (socket closed?)\n");
    	    }
    	    bzero(buffer,sizeof(buffer));
    	    n = read(sockfd,buffer,sizeof(buffer));
    	    if (n <= 0)
    	    {
				error("ERROR reading from socket (socket closed?)\n");
    	    }
    		type_ASAC_Zigbee_interface_command_reply *p_reply =(type_ASAC_Zigbee_interface_command_reply *) buffer;
    		switch (p_reply->code)
    		{
    			case enum_ASAC_ZigBee_interface_command_outside_send_message:
    			{
    				switch (p_reply->reply.outside_send_message.retcode)
    				{
    					case enum_ASAC_ZigBee_interface_command_outside_send_message_reply_retcode_OK:
    					{
    	    				printf("The message has been sent OK, id is %u\n", p_reply->reply.outside_send_message.id);
    						break;
    					}
    					default:
    					{
    	    				printf("ERROR SENDING THE MESSAGE: %i\n", p_reply->reply.outside_send_message.retcode);
    						break;
    					}
    				}
    				break;
    			}
    			case enum_ASAC_ZigBee_interface_command_unknown:
    			{
    				printf("THE SERVER DONT'T KNOW THE COMMAND: %i\n", p_reply->reply.unknown_reply.the_unknown_code);
    				break;
    			}
    			default:
    			{
    				printf("Unknown reply code: %i\n", p_reply->code);
    				break;
    			}
    		}
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
