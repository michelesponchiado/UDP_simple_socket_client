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

#include <termios.h>            //termios, TCSANOW, ECHO, ICANON
#include <unistd.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <time.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <pthread.h>
#include <poll.h>

#include "ASAC_ZigBee_network_commands.h"
#include "ASACSOCKET_check.h"

// the port number we use
#define def_use_secondary_port
#ifdef def_use_secondary_port
#define def_port_number 3118
#else
#define def_port_number 3117
#endif


void error(const char *msg)
{
    perror(msg);
    exit(0);
}
void prepare_formatted_message(char *buffer, unsigned int buffer_size, unsigned int message_index)
{
	int res = buffer_size;
	unsigned int used = 0;
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

int sockfd;
struct termios initial_settings,
               new_settings;

static void my_at_exit(void)
{
	tcsetattr(0, TCSANOW, &initial_settings);
	close(sockfd);
}

int main(int argc, char *argv[])
{
	int already_print_socket = 0;
	tcgetattr(0,&initial_settings);

	  new_settings = initial_settings;
	  new_settings.c_lflag &= ~ICANON;
	  new_settings.c_lflag &= ~ECHO;
	  new_settings.c_lflag &= ~ISIG;
	  new_settings.c_cc[VMIN] = 0;
	  new_settings.c_cc[VTIME] = 0;

	  tcsetattr(0, TCSANOW, &new_settings);
	atexit(my_at_exit);

    int portno;
    portno = def_port_number;
    if ((argc >= 2) && (strncasecmp(argv[1],"udpport=",8)==0))
    {
    	portno = atoi(&argv[1][8]);
    	printf("Forcing port %u\n",(unsigned int )portno);
    }
	printf("Using port %u\n",(unsigned int )portno);


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

    serv_addr.sin_addr.s_addr = INADDR_ANY;


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
	typedef struct _type_ep_cl
	{
		unsigned int ep,cl;
	}type_ep_cl;
	type_ep_cl ep_cl[1] =
	{
			{.ep = 1, .cl = 6},
	};

	{
    	unsigned int idx_loop_tx;
        for (idx_loop_tx = 0; idx_loop_tx < sizeof(ep_cl)/sizeof(ep_cl[0]); idx_loop_tx++)
    	{
    		type_ASAC_Zigbee_interface_request zmessage_tx;
		memset(&zmessage_tx, 0, sizeof(zmessage_tx));
    		zmessage_tx.code = enum_ASAC_ZigBee_interface_command_network_input_cluster_register_req;
    		unsigned int zmessage_size = def_size_ASAC_Zigbee_interface_req((&zmessage_tx),input_cluster_register);
    		type_ASAC_ZigBee_interface_network_input_cluster_register_req * p_ic_req = &zmessage_tx.req.input_cluster_register;
    		p_ic_req->endpoint = ep_cl[idx_loop_tx].ep;
    		p_ic_req->input_cluster_id = ep_cl[idx_loop_tx].cl;

    		type_struct_ASACSOCKET_msg amessage_tx;
    		memset(&amessage_tx,0,sizeof(amessage_tx));

    		unsigned int amessage_tx_size = 0;
    		enum_build_ASACSOCKET_formatted_message r_build = build_ASACSOCKET_formatted_message(&amessage_tx, (char *)&zmessage_tx, zmessage_size, &amessage_tx_size);
    		if (r_build == enum_build_ASACSOCKET_formatted_message_OK)
    		{
				unsigned int slen=sizeof(serv_addr);
				//send the message
				if (sendto(sockfd, (char*)&amessage_tx, amessage_tx_size , 0 , (struct sockaddr *) &serv_addr, slen)==-1)
				{
					printf("error on sendto()");
				}
				else
				{
					printf("endp/cluster registered OK: %u/%u\n",(unsigned int)p_ic_req->endpoint,(unsigned int)p_ic_req->input_cluster_id);
					cnt_msg_num++;
				}
    		}
    	}
#if 0
        for (idx_loop_tx = 0; idx_loop_tx < sizeof(ep_cl)/sizeof(ep_cl[0]); idx_loop_tx++)
    	{
    		type_ASAC_Zigbee_interface_request zmessage_tx = {0};
    		zmessage_tx.code = enum_ASAC_ZigBee_interface_command_network_input_cluster_unregister_req;
    		unsigned int zmessage_size = def_size_ASAC_Zigbee_interface_req((&zmessage_tx),input_cluster_unregister);
    		type_ASAC_ZigBee_interface_network_input_cluster_unregister_req * p_ic_req = &zmessage_tx.req.input_cluster_unregister;
    		p_ic_req->endpoint = ep_cl[idx_loop_tx].ep;
    		p_ic_req->input_cluster_id = ep_cl[idx_loop_tx].cl;

    		type_struct_ASACSOCKET_msg amessage_tx;
    		memset(&amessage_tx,0,sizeof(amessage_tx));

    		unsigned int amessage_tx_size = 0;
    		enum_build_ASACSOCKET_formatted_message r_build = build_ASACSOCKET_formatted_message(&amessage_tx, (char *)&zmessage_tx, zmessage_size, &amessage_tx_size);
    		if (r_build == enum_build_ASACSOCKET_formatted_message_OK)
    		{
				unsigned int slen=sizeof(serv_addr);
				//send the message
				if (sendto(sockfd, (char*)&amessage_tx, amessage_tx_size , 0 , (struct sockaddr *) &serv_addr, slen)==-1)
				{
					printf("error on sendto()");
				}
				else
				{
					printf("endp/cluster unregistered OK: %u/%u\n",(unsigned int)p_ic_req->endpoint,(unsigned int)p_ic_req->input_cluster_id);
					cnt_msg_num++;
				}
    		}
    	}
#endif
	}

	unsigned int idx_global_loop = 0;
    while(1)
    {
    	int c_from_kbd = getchar();
    	if (c_from_kbd == 'q' || c_from_kbd == 'Q')
    	{
    		break;
    	}
    	//char message[256];
    	++idx_global_loop;
#define def_send_outside_messages
#ifdef def_send_outside_messages
    	if ((c_from_kbd == 'm') || (c_from_kbd == 'M'))
    	{
    		static uint32_t idx_msg;
    		static char * the_messages[]=
    		{
    				"1: hello","2: how are you?","3: I hope all works well!"
    		};
    		if (++idx_msg >= sizeof(the_messages) / sizeof(the_messages[0]))
    		{
    			idx_msg = 0;
    		}
    		char *text_to_send = the_messages[idx_msg];
			printf("\n\n");
			printf("**********************************\n");
    		printf("%s: sending message: <%s>\n", __func__, text_to_send);
			printf("**********************************\n");
			printf("\n");
    		type_ASAC_Zigbee_interface_request zmessage_tx;
		memset(&zmessage_tx, 0, sizeof(zmessage_tx));
    		zmessage_tx.code = enum_ASAC_ZigBee_interface_command_outside_send_message;
    		unsigned int zmessage_size = def_size_ASAC_Zigbee_interface_req((&zmessage_tx),outside_send_message);
    		type_ASAC_ZigBee_interface_command_outside_send_message_req * p_req = &zmessage_tx.req.outside_send_message;
    		int len = snprintf((char*)p_req->message,sizeof(p_req->message),"%s", text_to_send);
    		type_ASAC_ZigBee_dst_id *pdst = &p_req->dst_id;
    		// end-device IEEE address
    		pdst->IEEE_destination_address = 0x124B0006E30188;
    		if (portno == 3172)
    		{
        		// coordinator IEEE address
    			pdst->IEEE_destination_address = 0x124B0006E2EE0B;
    		}
    		pdst->cluster_id = ep_cl[0].cl;
    		pdst->destination_endpoint = ep_cl[0].ep;
    		pdst->source_endpoint = ep_cl[0].ep;
    		pdst->transaction_id = 0;
    		if (len >= 0)
    		{
        		p_req->message_length = len;
        		type_struct_ASACSOCKET_msg amessage_tx;
        		memset(&amessage_tx,0,sizeof(amessage_tx));

        		unsigned int amessage_tx_size = 0;
        		enum_build_ASACSOCKET_formatted_message r_build = build_ASACSOCKET_formatted_message(&amessage_tx, (char *)&zmessage_tx, zmessage_size, &amessage_tx_size);
        		if (r_build == enum_build_ASACSOCKET_formatted_message_OK)
        		{
    				unsigned int slen=sizeof(serv_addr);
    				//send the message
    				if (sendto(sockfd, (char*)&amessage_tx, amessage_tx_size , 0 , (struct sockaddr *) &serv_addr, slen)==-1)
    				{
    					printf("%s: error on sendto()", __func__);
    				}
    				else
    				{
    					printf("%s: TX message OK: %*.*s\n",__func__,len, len, p_req->message);
    				}
        		}
    		}


    	}
    	else
#endif
    	if ((c_from_kbd == 'e') || (c_from_kbd == 'E'))
    	{
        	unsigned int idx_loop_tx;
        	//for (idx_loop_tx = 0; idx_loop_tx < 1+(rand()%8); idx_loop_tx++)
            for (idx_loop_tx = 0; idx_loop_tx < 1; idx_loop_tx++)
        	{
        		static unsigned int ui_cnt_echo;
        		type_ASAC_Zigbee_interface_request zmessage_tx;
			memset(&zmessage_tx, 0, sizeof(zmessage_tx));
        		zmessage_tx.code = enum_ASAC_ZigBee_interface_command_network_echo_req;
        		unsigned int zmessage_size = def_size_ASAC_Zigbee_interface_req((&zmessage_tx),echo);
        		type_ASAC_ZigBee_interface_network_echo_req * p_echo_req = &zmessage_tx.req.echo;
        		snprintf((char*)p_echo_req->message_to_echo,sizeof(p_echo_req->message_to_echo),"hello %u", ++ui_cnt_echo);

        		type_struct_ASACSOCKET_msg amessage_tx;
        		memset(&amessage_tx,0,sizeof(amessage_tx));

        		unsigned int amessage_tx_size = 0;
        		enum_build_ASACSOCKET_formatted_message r_build = build_ASACSOCKET_formatted_message(&amessage_tx, (char *)&zmessage_tx, zmessage_size, &amessage_tx_size);
        		if (r_build == enum_build_ASACSOCKET_formatted_message_OK)
        		{
    				unsigned int slen=sizeof(serv_addr);
    				//send the message
    				if (sendto(sockfd, (char*)&amessage_tx, amessage_tx_size , 0 , (struct sockaddr *) &serv_addr, slen)==-1)
    				{
    					printf("%s: error on sendto()", __func__);
    				}
    				else
    				{
    					printf("%s message sent OK: %s\n",__func__, p_echo_req->message_to_echo);
    					cnt_msg_num++;
    				}
        		}
        	}
    	}
        {
        	unsigned int idx_loop_rx;
#define def_pause_base_time_us 1000
#define def_loop_duration_time_ms 10
#define def_loop_rx_num_of (1 + (def_loop_duration_time_ms*1000)/def_pause_base_time_us)
        	for (idx_loop_rx = 0; idx_loop_rx < def_loop_rx_num_of; idx_loop_rx++)
        	{
        		// 10 ms pause
            	usleep(def_pause_base_time_us);
        		type_struct_ASACSOCKET_msg amessage_rx;
        		memset(&amessage_rx,0,sizeof(amessage_rx));

                socklen_t slen=sizeof(serv_addr);
                int n_received_bytes = 0;
                //try to receive some data, this is a blocking call
                n_received_bytes = recvfrom(sockfd, (char *)&amessage_rx, sizeof(amessage_rx), 0, (struct sockaddr *) &serv_addr, &slen) ;
                if (already_print_socket == 0)
                {
                	already_print_socket = 1;
        			char *ip = inet_ntoa(serv_addr.sin_addr);
        			printf("%s: socket ip:%s, port=%u\n", __func__,ip, (unsigned int)serv_addr.sin_port);
                }
                if (n_received_bytes == -1)
                {
                	//printf("error on recvfrom()");
                }
                else
                {
                	if (check_ASACSOCKET_formatted_message((char *)&amessage_rx, n_received_bytes) == enum_check_ASACSOCKET_formatted_message_OK)
                	{
                		type_ASAC_Zigbee_interface_command_reply *pzmessage_rx = (type_ASAC_Zigbee_interface_command_reply *)&(amessage_rx.body);
                		switch(pzmessage_rx->code)
                		{
                			case enum_ASAC_ZigBee_interface_command_network_echo_req:
                			{
                				type_ASAC_ZigBee_interface_network_echo_reply * p_echo_reply = &pzmessage_rx->reply.echo;
                				printf("\t%s: RX echo: %s\n", __func__,p_echo_reply->message_to_echo);
                				break;
                			}
                			case enum_ASAC_ZigBee_interface_command_network_input_cluster_register_req:
                			{
                				type_ASAC_ZigBee_interface_network_input_cluster_register_reply * p_icr_reply = &pzmessage_rx->reply.input_cluster_register;
                				printf("\t%s: RX endp/cluster register return code %u: %u/%u\n", __func__,(unsigned int)p_icr_reply->retcode, (unsigned int)p_icr_reply->endpoint, (unsigned int)p_icr_reply->input_cluster_id);
                				break;
                			}
                			case enum_ASAC_ZigBee_interface_command_network_input_cluster_unregister_req:
                			{
                				type_ASAC_ZigBee_interface_network_input_cluster_register_reply * p_icr_reply = &pzmessage_rx->reply.input_cluster_register;
                				printf("\t%s: RX endp/cluster unregister return code %u: %u/%u\n", __func__,(unsigned int)p_icr_reply->retcode, (unsigned int)p_icr_reply->endpoint, (unsigned int)p_icr_reply->input_cluster_id);
                				break;
                			}
                			case enum_ASAC_ZigBee_interface_command_outside_send_message:
                			{
                				type_ASAC_ZigBee_interface_command_outside_send_message_reply * p_reply = &pzmessage_rx->reply.outside_send_message;
                				printf("\t%s: RX send_message reply %u: %s\n",__func__, (unsigned int)p_reply->retcode,  (p_reply->retcode == enum_ASAC_ZigBee_interface_command_outside_send_message_reply_retcode_OK) ? "OK":"ERROR");
                				break;
                			}
                			case enum_ASAC_ZigBee_interface_command_outside_received_message:
                			{
                				type_ASAC_ZigBee_interface_command_received_message_callback * p = &pzmessage_rx->reply.received_message_callback;
                				printf("\n******************************\n\t%s: RX message callback: <", __func__);
                				{
                					unsigned int i;
                					for (i =0; i < p->message_length; i++)
                					{
                						char c = p->message[i];
                						if (isprint(c))
                						{
                							printf("%c",c);
                						}
                						else
                						{
                							printf("0x%X",(unsigned int)(c));
                						}
                					}
                					printf(">\n\n");
                				}
                				break;
                			}
                			default:
                			{
                				printf("%s: RX unknown message code: %X\n", __func__, pzmessage_rx->code);
                			}
                		}
                	}

                }
        	}
        }

    	usleep(100000);
    }
    close(sockfd);
    return 0;
}
