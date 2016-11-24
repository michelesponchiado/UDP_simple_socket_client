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
#include <inttypes.h>

#include "ASAC_ZigBee_network_commands.h"
#include "ASACSOCKET_check.h"

// the default port number we use
#define def_port_number 3117

#ifdef ANDROID
	#undef def_send_broadcast_packet
	#define def_use_local_host
#else
	#define def_send_broadcast_packet
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

#ifndef ANDROID
#define OK       0
#define NO_INPUT 1
#define TOO_LONG 2
static int getLine (char *prmpt, char *buff, size_t sz) {
    // Get line with buffer overrun protection.
    if (prmpt != NULL) {
        printf ("%s", prmpt);
        fflush (stdout);
    }
    uint32_t n_read = 0;
    while(1)
    {
        if (n_read + 1 >= sz)
        {
        	break;
        }
        fflush (stdin);
        int c = fgetc (stdin);
        if (c == EOF)
        {
        	continue;
        }
        if (c == 0x0d || c== '\n')
        {
        	break;
        }
        fputc(c, stdout);
        *buff++ = c&0xff;
        n_read++;
    }
    if (n_read == 0 || sz == 0)
        return NO_INPUT;
    if (n_read < sz )
    	*buff = 0;

    return OK;
}
#endif




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

#ifdef def_send_broadcast_packet
	struct sockaddr_in broadcastAddr;
	struct sockaddr_in broadcastAddr_rx;
	struct sockaddr_in broadcastAddr_rx_found;
	char *broadcastIP;
	unsigned short broadcastPort;
	int broadcastPermission;

	broadcastIP = "255.255.255.255";
	broadcastPort = portno;
	broadcastPermission = 1;
	if (setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, (void *) &broadcastPermission,sizeof(broadcastPermission)) < 0){
	   fprintf(stderr, "setsockopt error enabling broadcast permission!");
	   exit(1);
	}
	unsigned int num_reply_received_OK = 0;

	/* Construct local address structure */
	printf("Looking for server via broadcast messages\n");
	unsigned int idx_send_broadcast;
#define def_num_loop_try 5
    for (idx_send_broadcast = 0; idx_send_broadcast < def_num_loop_try; idx_send_broadcast++)
	{
    	printf("Loop %i of %i\n", (idx_send_broadcast+1), def_num_loop_try);
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
#if 0
		{
			FILE *f;
			f = fopen("echo.txt","wb");
			char *pc = (char*) &amessage_tx;
			unsigned int i;
			fprintf(f, "//Dump of the echo message with body=<%s>, the total message dump length is %u bytes\n",p_echo_req->message_to_echo, (unsigned int)amessage_tx_size);
			unsigned int num_bytes_written = 0;
			unsigned int just_return = 0;
			for (i = 0; i < amessage_tx_size; i++)
			{
				fprintf(f, "0x%02X ",(*pc++)&0xff);
				if (++ num_bytes_written >= 16)
				{
					num_bytes_written = 0;
					fprintf(f, "\r\n");
					just_return = 1;
				}
				else
				{
					just_return = 0;
				}
			}
			if (!just_return)
			{
				fprintf(f, "\r\n");
			}
			fclose(f);
		}
#endif
		if (r_build == enum_build_ASACSOCKET_formatted_message_OK)
		{
			memset(&broadcastAddr, 0, sizeof(broadcastAddr));
			broadcastAddr.sin_family = AF_INET;
			broadcastAddr.sin_addr.s_addr = inet_addr(broadcastIP);
			broadcastAddr.sin_port = htons(broadcastPort);
			unsigned int slen=sizeof(broadcastAddr);
			//send the message
			if (sendto(sockfd, (char*)&amessage_tx, amessage_tx_size , 0 , (struct sockaddr *) &broadcastAddr, slen)==-1)
			{
				printf("%s: error on broadcast sendto()\n", __func__);
			}
			else
			{
				printf("%s broadcast message sent OK: %s\n",__func__, p_echo_req->message_to_echo);
			}
		}

		unsigned int idx_loop_rx_broadcast_reply;
		unsigned int num_reply_received_actual_loop = 0;
#define def_broadcast_wait_time_ms 1000
#define def_broadcast_wait_pause_ms 1
#define def_broadcast_wait_pause_us (def_broadcast_wait_pause_ms * 1000)
#define def_loop_rx_broadcast_num_of (1 + def_broadcast_wait_time_ms/def_broadcast_wait_pause_ms)
    	for (idx_loop_rx_broadcast_reply = 0; idx_loop_rx_broadcast_reply < def_loop_rx_broadcast_num_of; idx_loop_rx_broadcast_reply++)
    	{
    		if (num_reply_received_actual_loop >= 1)
    		{
    			break;
    		}
        	usleep(def_broadcast_wait_pause_us);
    		type_struct_ASACSOCKET_msg amessage_rx;
    		memset(&amessage_rx,0,sizeof(amessage_rx));
            int n_received_bytes = 0;
        	broadcastAddr_rx = broadcastAddr;
            socklen_t slen=sizeof(broadcastAddr_rx);

            //try to receive some data, this is a blocking call
            n_received_bytes = recvfrom(sockfd, (char *)&amessage_rx, sizeof(amessage_rx), 0, (struct sockaddr *) &broadcastAddr_rx, &slen) ;
            if (n_received_bytes == -1)
            {
            	//printf("error on recvfrom()");
            }
            else
            {
                {
        			char *ip = inet_ntoa(broadcastAddr_rx.sin_addr);
        			printf("WOW BROADCAST REPLY! %s: socket ip:%s, port=%u\n", __func__,ip, (unsigned int)broadcastAddr_rx.sin_port);
                }
            	if (check_ASACSOCKET_formatted_message((char *)&amessage_rx, n_received_bytes) == enum_check_ASACSOCKET_formatted_message_OK)
            	{
            		type_ASAC_Zigbee_interface_command_reply *pzmessage_rx = (type_ASAC_Zigbee_interface_command_reply *)&(amessage_rx.body);
            		switch(pzmessage_rx->code)
            		{
            			case enum_ASAC_ZigBee_interface_command_network_echo_req:
            			{
            				num_reply_received_OK++;
            				num_reply_received_actual_loop = 0;
            				broadcastAddr_rx_found = broadcastAddr_rx;
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
	broadcastPermission = 0;
	if (setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, (void *) &broadcastPermission,sizeof(broadcastPermission)) < 0){
	   fprintf(stderr, "setsockopt error disabling broadcast permission!");
	   exit(1);
	}

    if (!num_reply_received_OK)
    {
    	printf("********* \n");
    	printf("********* \n");
    	printf("********* Hmm no server found!\n");
    	printf("********* \n");
    	printf("********* \n");
    	exit(0);
    }
    else
    {
    	printf("\n\nSome servers found!\n\n\n");

    }
#endif


    struct sockaddr_in serv_addr;

#ifdef def_send_broadcast_packet

    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(portno);
    serv_addr.sin_addr = broadcastAddr_rx_found.sin_addr;
	char *ip = inet_ntoa(serv_addr.sin_addr);
	printf("Server at address IP: %s\n", ip);
#else

//#define def_use_local_host
#ifdef def_use_local_host
    printf("getting local host name\n");
    struct hostent *server;
    server = gethostbyname("localhost");
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host\n");
        exit(0);
    }
    printf("local host name got OK\n");
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(portno);
    bcopy((char *)server->h_addr,
         (char *)&serv_addr.sin_addr.s_addr,
         server->h_length);
    serv_addr.sin_addr.s_addr = INADDR_ANY;
#else
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(portno);
//    inet_aton("10.0.0.9", &serv_addr.sin_addr);
    serv_addr.sin_addr.s_addr = inet_addr("10.0.0.9");
#endif
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
		printf("sending ep/cluster id loop %u\n", idx_loop_tx);
    
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
	uint32_t device_list_update_req = 0;
	uint32_t device_list_update_ack = 0;
#ifdef ANDROID
	uint32_t ui_fw_already_sent = 0;
	uint32_t ui_dl_already_sent = 0;
	uint32_t ui_ie_already_sent = 0;
#endif
	uint64_t IEEE_dst_address = 0;

	unsigned int idx_global_loop = 0;
	printf("main loop starts\n");
    while(1)
    {
#ifndef ANDROID
    	int c_from_kbd = getchar();
    	if (c_from_kbd == 'q' || c_from_kbd == 'Q')
    	{
    		break;
    	}
#endif
    	//char message[256];
    	++idx_global_loop;
#define def_send_outside_messages
#ifdef def_send_outside_messages
#ifndef ANDROID
    	// 'v' get server firmware version
    	if ((c_from_kbd == 'v') || (c_from_kbd == 'V'))
#else
    	// 'v' get server firmware version
    	if (idx_global_loop == 1 && !ui_fw_already_sent)
#endif
    	{

#ifdef ANDROID
		ui_fw_already_sent = 1;
#endif
    		device_list_update_ack = device_list_update_req;
    		type_ASAC_Zigbee_interface_request zmessage_tx;
    		memset(&zmessage_tx, 0, sizeof(zmessage_tx));
    		zmessage_tx.code = enum_ASAC_ZigBee_interface_command_network_firmware_version_req;
    		unsigned int zmessage_size = def_size_ASAC_Zigbee_interface_req((&zmessage_tx),firmware_version);
    		type_ASAC_ZigBee_interface_network_firmware_version_req * p_req = &zmessage_tx.req.firmware_version;
    		p_req->as_yet_unused = 0xa5;
    		{
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
    					printf("%s: (firmware version) error on sendto()", __func__);
    				}
    				else
    				{
    					printf("%s: (firmware version) TX message OK\n",__func__);
    				}
        		}
    		}

    	}


#ifndef ANDROID
    	// 'l' get devices list
    	if ((device_list_update_req != device_list_update_ack) || (c_from_kbd == 'l') || (c_from_kbd == 'L'))
#else
    	if (idx_global_loop == 6 && !ui_dl_already_sent)
#endif
    	{

#ifdef ANDROID
    		ui_dl_already_sent =1;
#endif
    		device_list_update_ack = device_list_update_req;
    		type_ASAC_Zigbee_interface_request zmessage_tx;
    		memset(&zmessage_tx, 0, sizeof(zmessage_tx));
    		zmessage_tx.code = enum_ASAC_ZigBee_interface_command_network_device_list_req;
    		unsigned int zmessage_size = def_size_ASAC_Zigbee_interface_req((&zmessage_tx),device_list);
    		type_ASAC_ZigBee_interface_network_device_list_req * p_req = &zmessage_tx.req.device_list;
    		p_req->sequence = 0;
    		p_req->start_index = 0;
    		{
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
    					printf("%s: (device list) error on sendto()", __func__);
    				}
    				else
    				{
    					printf("%s: (device list) TX message OK\n",__func__);
    				}
        		}
    		}

    	}

#ifndef ANDROID
    	// 'I' get my IEEE address
    	if ((c_from_kbd == 'I') || (c_from_kbd == 'i'))
#else
    	if (idx_global_loop == 2 && !ui_ie_already_sent)
#endif
    	{

#ifdef ANDROID
    		ui_ie_already_sent = 1;
#endif
    		device_list_update_ack = device_list_update_req;
    		type_ASAC_Zigbee_interface_request zmessage_tx;
    		memset(&zmessage_tx, 0, sizeof(zmessage_tx));
    		zmessage_tx.code = enum_ASAC_ZigBee_interface_command_network_my_IEEE_req;
    		unsigned int zmessage_size = def_size_ASAC_Zigbee_interface_req((&zmessage_tx),my_IEEE);
    		type_ASAC_ZigBee_interface_network_my_IEEE_req * p_req = &zmessage_tx.req.my_IEEE;
    		p_req->unused = 0;
    		{
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
    					printf("%s: (my IEEE) error on sendto()", __func__);
    				}
    				else
    				{
    					printf("%s: (my IEEE) TX message OK\n",__func__);
    				}
        		}
    		}

    	}

#ifndef ANDROID
    	// 'U' get link quality
    	if ((c_from_kbd == 'U') || (c_from_kbd == 'u'))
    	{

    		type_ASAC_Zigbee_interface_request zmessage_tx;
    		memset(&zmessage_tx, 0, sizeof(zmessage_tx));
    		zmessage_tx.code = enum_ASAC_ZigBee_interface_command_network_signal_strength_req;
    		unsigned int zmessage_size = def_size_ASAC_Zigbee_interface_req((&zmessage_tx),signal_strength);
    		{
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
    					printf("%s: (my IEEE) error on sendto()", __func__);
    				}
    				else
    				{
    					printf("%s: (my IEEE) TX message OK\n",__func__);
    				}
        		}
    		}

    	}
#endif

#ifndef ANDROID
    	// @ sends user message
    	if (c_from_kbd == '@')
    	{

    		char buff[129];
    		memset(buff, 0, sizeof(buff));
    		if (getLine ("text to send:", buff, sizeof(buff) - 1) == OK)
    		{
    			printf("\n\n");
    			printf("**********************************\n");
        		printf("%s: sending message: <%s>\n", __func__, buff);
    			printf("**********************************\n");
    			printf("\n");
        		type_ASAC_Zigbee_interface_request zmessage_tx;
        		memset(&zmessage_tx, 0, sizeof(zmessage_tx));
        		zmessage_tx.code = enum_ASAC_ZigBee_interface_command_outside_send_message;
        		unsigned int zmessage_size = def_size_ASAC_Zigbee_interface_req((&zmessage_tx),outside_send_message);
        		type_ASAC_ZigBee_interface_command_outside_send_message_req * p_req = &zmessage_tx.req.outside_send_message;
        		int len = snprintf((char*)p_req->message,sizeof(p_req->message),"%s", buff);
        		type_ASAC_ZigBee_dst_id *pdst = &p_req->dst_id;
    #ifdef ANDROID
        		if (IEEE_dst_address)
        		{
        			pdst->IEEE_destination_address = IEEE_dst_address;
        		}
        		else
        		{
        			pdst->IEEE_destination_address = 0x124B0006E2EE0B;
        		}
    #else
        		// end-device IEEE address
        		pdst->IEEE_destination_address = 0x124B0006E30188;
        		if (portno == 3172)
        		{
            		// coordinator IEEE address
        			pdst->IEEE_destination_address = 0x124B0006E2EE0B;
        		}
        		if (IEEE_dst_address)
        		{
        			pdst->IEEE_destination_address = IEEE_dst_address;
        		}
    #endif
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


    	}
    	else
#endif



#ifdef ANDROID
	usleep(1000);
	if (idx_global_loop == 5000)
#else
    	if ((c_from_kbd == 'm') || (c_from_kbd == 'M'))
#endif
    	{
#ifdef ANDROID
	idx_global_loop = 0;
#endif
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
#ifdef ANDROID
    		if (IEEE_dst_address)
    		{
    			pdst->IEEE_destination_address = IEEE_dst_address;
    		}
    		else
    		{
    			pdst->IEEE_destination_address = 0x124B0006E2EE0B;
    		}
#else
    		// end-device IEEE address
    		pdst->IEEE_destination_address = 0x124B0006E30188;
    		if (portno == 3172)
    		{
        		// coordinator IEEE address
    			pdst->IEEE_destination_address = 0x124B0006E2EE0B;
    		}
    		if (IEEE_dst_address)
    		{
    			pdst->IEEE_destination_address = IEEE_dst_address;
    		}
#endif
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

#ifdef ANDROID
	if (idx_global_loop == 2500)
#else
    	if ((c_from_kbd == 'e') || (c_from_kbd == 'E'))
#endif
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

#ifndef ANDROID
        	for (idx_loop_rx = 0; idx_loop_rx < def_loop_rx_num_of; idx_loop_rx++)
        	{
        		// 10 ms pause
            	usleep(def_pause_base_time_us);
#else
        	for (idx_loop_rx = 0; idx_loop_rx < 1; idx_loop_rx++)
        	{
#endif

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
                			case enum_ASAC_ZigBee_interface_command_network_firmware_version_req:
                			{
                				type_ASAC_ZigBee_interface_network_firmware_version_reply * p_reply = &pzmessage_rx->reply.firmware_version;
                				printf("%s: server firmware version is: %s\n",__func__, p_reply->string);
                				break;
                			}
                			case enum_ASAC_ZigBee_interface_command_device_list_changed_signal:
                			{
                				device_list_update_req++;
                				type_ASAC_ZigBee_interface_network_device_list_changed_signal * p_signal = &pzmessage_rx->reply.device_list_changed;
                				printf("%s: device list has changed to sequence number %u\n",__func__, p_signal->sequence_number);
                				break;
                			}
                			case enum_ASAC_ZigBee_interface_command_network_signal_strength_req:
                			{
                				type_ASAC_ZigBee_interface_network_signal_strength_reply * p_reply = &pzmessage_rx->reply.signal_strength;
                				char *pc = "unknown";
                				switch(p_reply->level_min0_max4)
                				{
                					case 0:
                					default:
                					{
                						break;
                					}
                					case 1:
                					{
                						pc = "low";
                					}
                					case 2:
                					{
                						pc = "normal";
                					}
                					case 3:
                					{
                						pc = "good";
                					}
                					case 4:
                					{
                						pc = "excellent";
                					}
                				}
                				printf("%s: signal strength is %s (%u / %i dBm), %" PRIi64 " ms ago\n",__func__, pc, p_reply->level_min0_max4, p_reply->v_dBm, p_reply->milliseconds_ago);
                				break;
                			}
                			case enum_ASAC_ZigBee_interface_command_network_device_list_req:
                			{
                				type_ASAC_ZigBee_interface_network_device_list_reply * p_reply = &pzmessage_rx->reply.device_list;
                				printf("device list:\n");
                				printf("\t sequence valid : %u\n", p_reply->sequence_valid);
                				printf("\t start index    : %u\n", p_reply->start_index);
                				printf("\t list ends here : %u\n", p_reply->list_ends_here);
                				printf("\t # of devices	   : %u\n", p_reply->num_devices_in_chunk);
                				unsigned int i;
                				for (i = 0; i < p_reply->num_devices_in_chunk; i++)
                				{
                    				printf("\t IEEE address %02i: 0x%" PRIx64 "\n", i, p_reply->list_chunk[i].IEEE_address);
                				}
                				if (p_reply->num_devices_in_chunk > 0)
                				{
                    				IEEE_dst_address = p_reply->list_chunk[0].IEEE_address;
                				}
                				break;
                			}
                			case enum_ASAC_ZigBee_interface_command_network_my_IEEE_req:
                			{
                				type_ASAC_ZigBee_interface_network_my_IEEE_reply * p_reply = &pzmessage_rx->reply.my_IEEE;
                				printf("my IEEE address is %s:\n", p_reply->is_valid_IEEE_address ? "valid":"NOT valid");
                				if (p_reply->is_valid_IEEE_address)
                				{
                    				printf("\t my IEEE address is 0x%" PRIx64 "\n", p_reply->IEEE_address);
                				}
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
#ifndef ANDROID
    	usleep(100000);
#endif
    }
    close(sockfd);
    return 0;
}
