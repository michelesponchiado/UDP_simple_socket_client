/*
 * main_client.c
 *
 *  Created on: Apr 20, 2016
 *      Author: michele
 */

#ifndef _GNU_SOURCE
	#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>

#include <sys/ioctl.h>
#include <sys/time.h>
#include <pthread.h>
#include <poll.h>

#include "ASAC_ZigBee_network_commands.h"
// the port number we use
#define def_port_number 3117

void error(const char *msg)
{
    perror(msg);
    exit(0);
}
void prepare_formatted_message(char *buffer, unsigned int buffer_size, unsigned int message_index)
{
	int res = buffer_size;
	int used = 0;
	char * pc = buffer;
	used += snprintf(pc,res,"STX %4i ETX",message_index);
	pc += used;
	res -= used;
	if(res > 0)
	{
		unsigned int crc = 0;
		unsigned int idx;
		for (idx = 0; idx < used; idx++)
		{
			crc ^= buffer[idx];
		}
		crc ^= 0xff;
		crc &= 0xff;
		used += snprintf(pc,res,"%4u",crc);
	}
}

unsigned int is_OK_check_formatted_message(char *buffer, unsigned int buffer_size)
{
	int index;
	unsigned int crc_rx;
	int n_args = sscanf(buffer,"STX %i ETX%u",&index,&crc_rx);
	if (n_args != 2)
	{
		return 0;
	}
	{
		unsigned int crc = 0;
		unsigned int idx;
		for (idx = 0; idx < strlen(buffer) - 4; idx++)
		{
			crc ^= buffer[idx];
		}
		crc ^= 0xff;
		crc &= 0xff;
		if (crc != crc_rx)
		{
			return 0;
		}
	}
	return 1;
}


int main(int argc, char *argv[])
{
    int sockfd, portno;


    portno = def_port_number;
    sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sockfd < 0)
        error("ERROR opening socket");
	// mark the socket as NON blocking
	{
		int flags = fcntl(sockfd, F_GETFL, 0);
		fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);
	}

#define def_use_local_host
#ifdef def_use_local_host
    struct hostent *server;
    server = gethostbyname("localhost");
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host\n");
        exit(0);
    }
    struct sockaddr_in serv_addr;
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr,
         (char *)&serv_addr.sin_addr.s_addr,
         server->h_length);
    serv_addr.sin_port = htons(portno);
#else
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(portno);
    inet_aton("192.168.0.208", &serv_addr.sin_addr);
#endif
//#define def_local_check
#ifdef def_local_check
    {
    	char message[256];
    	int i;
    	for (i = 0; i<256; i++)
    	{
    		prepare_formatted_message(message,sizeof(message), i);
        	if (is_OK_check_formatted_message(message, sizeof(message)))
        	{
            	printf("OK message received: %s\n",message);
        	}
        	else
        	{
            	printf("ERR message received: %s\n",message);
            	exit(0);
        	}
    	}
    }
#endif

    unsigned int cnt_msg_num = 0;
    while(1)
    {
    	char message[256];

    	{
        	unsigned int idx_loop_tx;
        	for (idx_loop_tx = 0; idx_loop_tx < 1+(rand()%8); idx_loop_tx++)
        	{
				prepare_formatted_message(message,sizeof(message), cnt_msg_num);
				unsigned int slen=sizeof(serv_addr);
				//send the message
				if (sendto(sockfd, message, strlen(message)+1 , 0 , (struct sockaddr *) &serv_addr, slen)==-1)
				{
					printf("error on sendto()");
				}
				else
				{
					printf("message sent OK: %s\n",message);
					cnt_msg_num++;
				}
        	}
    	}
        {
        	unsigned int idx_loop_rx;
#define def_pause_base_time_us 10000
#define def_loop_duration_time_ms 1500
#define def_loop_rx_num_of ((def_loop_duration_time_ms*1000)/def_pause_base_time_us)
        	for (idx_loop_rx = 0; idx_loop_rx < def_loop_rx_num_of; idx_loop_rx++)
        	{
        		// 10 ms pause
            	usleep(def_pause_base_time_us);
                //receive a reply and print it
                //clear the buffer by filling null, it might have previously received data
                memset(message,'\0', sizeof(message));
                unsigned int slen=sizeof(serv_addr);
                //try to receive some data, this is a blocking call
                if (recvfrom(sockfd, message, sizeof(message), 0, (struct sockaddr *) &serv_addr, &slen) == -1)
                {
                	//printf("error on recvfrom()");
                }
                else
                {
                	if (is_OK_check_formatted_message(message, sizeof(message)))
                	{
                    	printf("OK message received: %s\n",message);
                	}
                	else
                	{
                    	printf("ERR message received: %s\n",message);
                    	exit(0);
                	}
                }
        	}
        }

    	usleep(100000);
    }
    close(sockfd);
    return 0;
}
