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


static uint32_t calcCrcLikeChip(const unsigned char *pData, unsigned long ulByteCount)
{
    uint32_t d, ind;
    uint32_t acc = 0xFFFFFFFF;
    const uint32_t ulCrcRand32Lut[] =
    {
        0x00000000, 0x1DB71064, 0x3B6E20C8, 0x26D930AC,
        0x76DC4190, 0x6B6B51F4, 0x4DB26158, 0x5005713C,
        0xEDB88320, 0xF00F9344, 0xD6D6A3E8, 0xCB61B38C,
        0x9B64C2B0, 0x86D3D2D4, 0xA00AE278, 0xBDBDF21C
    };

    while ( ulByteCount-- )
    {
        d = *pData++;
        ind = (acc & 0x0F) ^ (d & 0x0F);
        acc = (acc >> 4) ^ ulCrcRand32Lut[ind];
        ind = (acc & 0x0F) ^ (d >> 4);
        acc = (acc >> 4) ^ ulCrcRand32Lut[ind];
    }

    return (acc ^ 0xFFFFFFFF);
}

static uint32_t get_new_link_id(void)
{
	static uint32_t cur_id = defFirstValidLinkId;
	uint32_t id = cur_id;
	uint32_t new_id = cur_id;
	if (new_id >= defLastValidLinkId)
	{
		new_id = defFirstValidLinkId;
	}
	else
	{
		new_id++;
	}
	cur_id = new_id;
	return id;
}

static void init_header_protocol(type_ASAC_Zigbee_interface_protocol_header *p)
{
	p->major_protocol_id = def_ASACZ_ZIGBEE_NETWORK_COMMANDS_PROTOCOL_MAJOR_VERSION;
	p->minor_protocol_id = def_ASACZ_ZIGBEE_NETWORK_COMMANDS_PROTOCOL_MINOR_VERSION;
}
static void init_header(type_ASAC_Zigbee_interface_header *p, uint32_t command_version, uint32_t command_code, uint32_t link_id)
{
	init_header_protocol(&p->p);
	type_ASAC_Zigbee_interface_command_header *pc = &p->c;
	pc->command_version = command_version;
	pc->command_code = command_code;
	pc->command_link_id = link_id;
}

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
#include <libgen.h>

#define my_version "0.1.4"

int main(int argc, char *argv[])
{

#if 0
	{
		{
			type_ASAC_ZigBee_interface_command_fwupd_req req;

			printf("type_ASAC_ZigBee_interface_command_fwupd_req: size = %lu\n", sizeof(req));
			printf("\tdst:  offset %-4lu, size %-4lu\n", __builtin_offsetof(type_ASAC_ZigBee_interface_command_fwupd_req,dst), sizeof(req.dst));
			printf("\t  enum_dst:    offset %-4lu, size %-4lu\n", __builtin_offsetof(type_ASAC_ZigBee_interface_command_fwupd_req,dst.enum_dst), sizeof(req.dst.enum_dst));
			printf("\n");
			printf("\t  uint32_dst:  offset %-4lu, size %-4lu\n", __builtin_offsetof(type_ASAC_ZigBee_interface_command_fwupd_req,dst.uint32_dst), sizeof(req.dst.uint32_dst));
			printf("\n");
			printf("\top:   offset %-4lu, size %-4lu\n", __builtin_offsetof(type_ASAC_ZigBee_interface_command_fwupd_req,ops), sizeof(req.ops));
			printf("\t  CC2650:      offset %-4lu, size %-4lu\n", __builtin_offsetof(type_ASAC_ZigBee_interface_command_fwupd_req,ops.CC2650), sizeof(req.ops.CC2650));
			printf("\n");
			printf("\t  uint32_op:   offset %-4lu, size %-4lu\n", __builtin_offsetof(type_ASAC_ZigBee_interface_command_fwupd_req,ops.uint32_op), sizeof(req.ops.uint32_op));
			printf("\n");
			printf("\tbody: offset %-4lu, size %-4lu\n", __builtin_offsetof(type_ASAC_ZigBee_interface_command_fwupd_req,body), sizeof(req.body));
			printf("\t  CC2650_read_firmware_version:       offset %-4lu, size %-4lu\n", __builtin_offsetof(type_ASAC_ZigBee_interface_command_fwupd_req,body.CC2650_read_firmware_version), sizeof(req.body.CC2650_read_firmware_version));
			printf("\t    unused: offset %-4lu, size %-4lu\n", __builtin_offsetof(type_ASAC_ZigBee_interface_command_fwupd_req,body.CC2650_read_firmware_version.unused), sizeof(req.body.CC2650_read_firmware_version.unused));
			printf("\n");
			printf("\t  CC2650_start_firmware_update:       offset %-4lu, size %-4lu\n", __builtin_offsetof(type_ASAC_ZigBee_interface_command_fwupd_req,body.CC2650_start_firmware_update), sizeof(req.body.CC2650_start_firmware_update));
			printf("\t    CC2650_fw_signed_filename: offset %-4lu, size %-4lu\n", __builtin_offsetof(type_ASAC_ZigBee_interface_command_fwupd_req,body.CC2650_start_firmware_update.CC2650_fw_signed_filename), sizeof(req.body.CC2650_start_firmware_update.CC2650_fw_signed_filename));
			printf("\n");
			printf("\t  CC2650_query_firmware_update_status:offset %-4lu, size %-4lu\n", __builtin_offsetof(type_ASAC_ZigBee_interface_command_fwupd_req,body.CC2650_query_firmware_update_status), sizeof(req.body.CC2650_query_firmware_update_status));
			printf("\t    unused: offset %-4lu, size %-4lu\n", __builtin_offsetof(type_ASAC_ZigBee_interface_command_fwupd_req,body.CC2650_query_firmware_update_status.unused), sizeof(req.body.CC2650_query_firmware_update_status.unused));
			printf("\n");
			printf("\t  CC2650_query_firmware_file_req:     offset %-4lu, size %-4lu\n", __builtin_offsetof(type_ASAC_ZigBee_interface_command_fwupd_req,body.CC2650_query_firmware_file_req), sizeof(req.body.CC2650_query_firmware_file_req));
			printf("\t    CC2650_fw_query_filename: offset %-4lu, size %-4lu\n", __builtin_offsetof(type_ASAC_ZigBee_interface_command_fwupd_req,body.CC2650_query_firmware_file_req.CC2650_fw_query_filename), sizeof(req.body.CC2650_query_firmware_file_req.CC2650_fw_query_filename));
			printf("\n");
		}

		{
			type_ASAC_ZigBee_interface_command_fwupd_reply reply;

			printf("type_ASAC_ZigBee_interface_command_fwupd_reply size = %lu\n", sizeof(reply));
			printf("\tdst:  offset %-4lu, size %-4lu\n", __builtin_offsetof(type_ASAC_ZigBee_interface_command_fwupd_reply,dst), sizeof(reply.dst));
			printf("\t  enum_dst:    offset %-4lu, size %-4lu\n", __builtin_offsetof(type_ASAC_ZigBee_interface_command_fwupd_reply,dst.enum_dst), sizeof(reply.dst.enum_dst));
			printf("\n");
			printf("\t  uint32_dst:  offset %-4lu, size %-4lu\n", __builtin_offsetof(type_ASAC_ZigBee_interface_command_fwupd_reply,dst.uint32_dst), sizeof(reply.dst.uint32_dst));
			printf("\n");
			printf("\top:   offset %-4lu, size %-4lu\n", __builtin_offsetof(type_ASAC_ZigBee_interface_command_fwupd_reply,ops), sizeof(reply.ops));
			printf("\t  CC2650:     offset %-4lu, size %-4lu\n", __builtin_offsetof(type_ASAC_ZigBee_interface_command_fwupd_reply,ops.CC2650), sizeof(reply.ops.CC2650));
			printf("\n");
			printf("\t  uint32_op:  offset %-4lu, size %-4lu\n", __builtin_offsetof(type_ASAC_ZigBee_interface_command_fwupd_reply,ops.uint32_op), sizeof(reply.ops.uint32_op));
			printf("\n");
			printf("\tbody: offset %-4lu, size %-4lu\n", __builtin_offsetof(type_ASAC_ZigBee_interface_command_fwupd_reply,body), sizeof(reply.body));
			printf("\t  CC2650_read_firmware_version: offset %-4lu, size %-4lu\n", __builtin_offsetof(type_ASAC_ZigBee_interface_command_fwupd_reply,body.CC2650_read_firmware_version), sizeof(reply.body.CC2650_read_firmware_version));
#undef def_complete(field)
#define def_complete(field) __builtin_offsetof(type_ASAC_ZigBee_interface_command_fwupd_reply,body.CC2650_read_firmware_version.field), sizeof(reply.body.CC2650_read_firmware_version.field)
			printf("\t    is_valid: offset %-4lu, size %-4lu\n", def_complete(is_valid));
			printf("\t    major:    offset %-4lu, size %-4lu\n", def_complete(major));
			printf("\t    middle:   offset %-4lu, size %-4lu\n", def_complete(middle));
			printf("\t    minor:    offset %-4lu, size %-4lu\n", def_complete(minor));
			printf("\t    product:  offset %-4lu, size %-4lu\n", def_complete(product));
			printf("\t    transport:offset %-4lu, size %-4lu\n", def_complete(transport));
			printf("\n");

			printf("\t  CC2650_start_firmware_update: offset %-4lu, size %-4lu\n", __builtin_offsetof(type_ASAC_ZigBee_interface_command_fwupd_reply,body.CC2650_start_firmware_update), sizeof(reply.body.CC2650_start_firmware_update));
#undef def_complete
#define def_complete(field) __builtin_offsetof(type_ASAC_ZigBee_interface_command_fwupd_reply,body.CC2650_start_firmware_update.field), sizeof(reply.body.CC2650_start_firmware_update.field)
			printf("\t    is_OK:         offset %-4lu, size %-4lu\n", def_complete(is_OK));
			printf("\t    num_request:   offset %-4lu, size %-4lu\n", def_complete(num_request));
			printf("\t    result_code:   offset %-4lu, size %-4lu\n", def_complete(result_code));
			printf("\t    result_message:offset %-4lu, size %-4lu\n", def_complete(result_message));
			printf("\n");

			printf("\t  CC2650_query_firmware_update_status: offset %-4lu, size %-4lu\n", __builtin_offsetof(type_ASAC_ZigBee_interface_command_fwupd_reply,body.CC2650_query_firmware_update_status), sizeof(reply.body.CC2650_query_firmware_update_status));
#undef def_complete
#define def_complete(field) __builtin_offsetof(type_ASAC_ZigBee_interface_command_fwupd_reply,body.CC2650_query_firmware_update_status.field), sizeof(reply.body.CC2650_query_firmware_update_status.field)
			printf("\t    status:                        offset %-4lu, size %-4lu\n",def_complete(status));
			printf("\t    ends_OK:                       offset %-4lu, size %-4lu\n", def_complete(ends_OK));
			printf("\t    ends_ERR:                      offset %-4lu, size %-4lu\n", def_complete(fw_update_result_code_is_valid));
			printf("\t    fw_update_result_code_is_valid:offset %-4lu, size %-4lu\n", def_complete(fw_update_result_code));
			printf("\t    fw_update_result_code:         offset %-4lu, size %-4lu\n", def_complete(status));
			printf("\t    fw_update_result_string:       offset %-4lu, size %-4lu\n", def_complete(fw_update_result_string));
			printf("\t    flash_write_percentage:        offset %-4lu, size %-4lu\n", def_complete(flash_write_percentage));
			printf("\t    num_request:                   offset %-4lu, size %-4lu\n", def_complete(num_request));
			printf("\t    num_ack:                       offset %-4lu, size %-4lu\n", def_complete(num_ack));
			printf("\n");

			printf("\t  CC2650_query_firmware_file_reply: offset %-4lu, size %-4lu\n", __builtin_offsetof(type_ASAC_ZigBee_interface_command_fwupd_reply,body.CC2650_query_firmware_file_reply), sizeof(reply.body.CC2650_query_firmware_file_reply));
#undef def_complete
#define def_complete(field) __builtin_offsetof(type_ASAC_ZigBee_interface_command_fwupd_reply,body.CC2650_query_firmware_file_reply.field), sizeof(reply.body.CC2650_query_firmware_file_reply.field)
			printf("\t    retcode:                   offset %-4lu, size %-4lu\n",def_complete(retcode));
			printf("\t    query_result_string:       offset %-4lu, size %-4lu\n",def_complete(query_result_string));
			printf("\t    CC2650_fw_query_filename:  offset %-4lu, size %-4lu\n",def_complete(CC2650_fw_query_filename));
			printf("\t    magic_name:                offset %-4lu, size %-4lu\n",def_complete(magic_name));
			printf("\t    ascii_fw_type:             offset %-4lu, size %-4lu\n",def_complete(ascii_fw_type));
			printf("\t    ascii_version_number:      offset %-4lu, size %-4lu\n",def_complete(ascii_version_number));
			printf("\t    date:                      offset %-4lu, size %-4lu\n",def_complete(date));
			printf("\t    fw_type:                   offset %-4lu, size %-4lu\n",def_complete(fw_type));
			printf("\t    fw_version_major:          offset %-4lu, size %-4lu\n",def_complete(fw_version_major));
			printf("\t    fw_version_middle:         offset %-4lu, size %-4lu\n",def_complete(fw_version_middle));
			printf("\t    fw_version_minor:          offset %-4lu, size %-4lu\n",def_complete(fw_version_minor));
			printf("\t    firmware_body_size:        offset %-4lu, size %-4lu\n",def_complete(firmware_body_size));
			printf("\t    firmware_body_CRC32_CC2650:offset %-4lu, size %-4lu\n",def_complete(firmware_body_CRC32_CC2650));
			printf("\t    header_CRC32_CC2650:       offset %-4lu, size %-4lu\n",def_complete(header_CRC32_CC2650));
			printf("\n");
		}

		return 0;
	}
#endif

	char my_name[1024];
	memset(my_name, 0, sizeof(my_name));
	char *pc_basename = "UDP_ASACZ_client";

	uint64_t available_IEEE_addresses[128];
	uint32_t numof_available_IEEE_addresses = 0;
	memset(&available_IEEE_addresses, 0, sizeof(available_IEEE_addresses));
	{
		snprintf(my_name, sizeof(my_name), "%s", argv[0]);
		pc_basename = basename(my_name);
		if (!pc_basename)
		{
			pc_basename = "UDP_ASACZ_client";
		}
		printf("%s version %s %s\n",pc_basename, my_version, __DATE__);
	}

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
		init_header(&zmessage_tx.h, def_echo_req_command_version, enum_ASAC_ZigBee_interface_command_network_echo_req, get_new_link_id());
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
            		switch(pzmessage_rx->h.c.command_code)
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
            				printf("%s: RX unknown message code: %X\n", __func__, pzmessage_rx->h.c.command_code);
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

if (1)
{
	type_ASAC_Zigbee_interface_request zmessage_tx;
	memset(&zmessage_tx, 0, sizeof(zmessage_tx));
	init_header(&zmessage_tx.h, def_my_IEEE_req_command_version, enum_ASAC_ZigBee_interface_command_network_my_IEEE_req, get_new_link_id());
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
    		init_header(&zmessage_tx.h, def_input_cluster_register_req_command_version, enum_ASAC_ZigBee_interface_command_network_input_cluster_register_req, get_new_link_id());
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
#ifndef ANDROID_NO_CONSOLE
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
#ifndef ANDROID_NO_CONSOLE
    	// 'v' get server firmware version
    	if ((c_from_kbd == 'v') || (c_from_kbd == 'V'))
#else
    	// 'v' get server firmware version
    	if (idx_global_loop == 1 && !ui_fw_already_sent)
#endif
    	{

#ifdef ANDROID_NO_CONSOLE
		ui_fw_already_sent = 1;
#endif

#if 0
    		device_list_update_ack = device_list_update_req;
    		type_ASAC_Zigbee_interface_request zmessage_tx;
    		memset(&zmessage_tx, 0, sizeof(zmessage_tx));
    		init_header(&zmessage_tx.h, def_firmware_version_req_command_version, enum_ASAC_ZigBee_interface_command_network_firmware_version_req, get_new_link_id());
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
#else
    		type_ASAC_Zigbee_interface_request zmessage_tx;
    		memset(&zmessage_tx, 0, sizeof(zmessage_tx));
    		init_header(&zmessage_tx.h, def_firmware_version_req_command_version, enum_ASAC_ZigBee_interface_command_network_probe, get_new_link_id());
    		unsigned int zmessage_size = def_size_ASAC_Zigbee_interface_req((&zmessage_tx),network_probe_request);
    		type_ASAC_ZigBee_interface_network_probe_request * p_req = &zmessage_tx.req.network_probe_request;
    		p_req->op.enum_op = enum_network_probe_op_discovery;
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
    					printf("%s: (discovery version) error on sendto()", __func__);
    				}
    				else
    				{
    					printf("%s: (discovery version) TX message OK\n",__func__);
    				}
        		}
    		}
#endif
    	}


#ifndef ANDROID_NO_CONSOLE
    	// 'l' get devices list
    	if ((device_list_update_req != device_list_update_ack) || (c_from_kbd == 'l') || (c_from_kbd == 'L'))
#else
    	if (idx_global_loop == 6 && !ui_dl_already_sent)
#endif
    	{

#ifdef ANDROID_NO_CONSOLE
    		ui_dl_already_sent =1;
#endif
    		device_list_update_ack = device_list_update_req;
    		type_ASAC_Zigbee_interface_request zmessage_tx;
    		memset(&zmessage_tx, 0, sizeof(zmessage_tx));
    		init_header(&zmessage_tx.h, def_device_list_req_command_version, enum_ASAC_ZigBee_interface_command_network_device_list_req, get_new_link_id());
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

#ifndef ANDROID_NO_CONSOLE
    	// 'I' get my IEEE address
    	if ((c_from_kbd == 'I') || (c_from_kbd == 'i'))
#else
    	if (idx_global_loop == 2 && !ui_ie_already_sent)
#endif
    	{

#ifdef ANDROID_NO_CONSOLE
    		ui_ie_already_sent = 1;
#endif
    		type_ASAC_Zigbee_interface_request zmessage_tx;
    		memset(&zmessage_tx, 0, sizeof(zmessage_tx));
    		init_header(&zmessage_tx.h, def_my_IEEE_req_command_version, enum_ASAC_ZigBee_interface_command_network_my_IEEE_req, get_new_link_id());
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

#ifndef ANDROID_NO_CONSOLE
    	// 'U' get link quality
    	if ((c_from_kbd == 'U') || (c_from_kbd == 'u'))
    	{

    		type_ASAC_Zigbee_interface_request zmessage_tx;
    		memset(&zmessage_tx, 0, sizeof(zmessage_tx));
    		init_header(&zmessage_tx.h, def_signal_strength_req_command_version, enum_ASAC_ZigBee_interface_command_network_signal_strength_req, get_new_link_id());
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

#ifndef ANDROID_NO_CONSOLE
    	// 'F' get CC2650 firmware version
    	if ((c_from_kbd == 'F') || (c_from_kbd == 'f'))
    	{

    		printf("0 read fw version, 1 start fw update, 2 req fw update status 3 req firmware file info 4 update ASACZ 5 status upd ASACZ 9 exit\n");
    		int option = 0;
    		while(1)
    		{
        		c_from_kbd = getchar();
        		if ((c_from_kbd == '0') || (c_from_kbd == '1') || (c_from_kbd == '2') || (c_from_kbd == '3')|| (c_from_kbd == '4')|| (c_from_kbd == '5'))
        		{
            		option = c_from_kbd;
            		break;
        		}
        		if (c_from_kbd == '9')
        		{
        			break;
        		}

    		}
    		if (option > 0)
    		{
        		type_ASAC_Zigbee_interface_request zmessage_tx;
        		type_ASAC_ZigBee_interface_command_fwupd_req * p_req = NULL;
        		unsigned int zmessage_size = 0;
        		memset(&zmessage_tx, 0, sizeof(zmessage_tx));
        		init_header(&zmessage_tx.h, def_signal_strength_req_command_version, enum_ASAC_ZigBee_interface_command_administrator_firmware_update, get_new_link_id());
        		zmessage_size = def_size_ASAC_Zigbee_interface_req((&zmessage_tx), fwupd_req);
        		p_req = &zmessage_tx.req.fwupd_req;
        		p_req->dst.enum_dst = enum_ASAC_ZigBee_fwupd_destination_CC2650;
        		switch (c_from_kbd)
        		{
        			case '0':
        			default:
        			{
        	    		p_req->ops.CC2650= enum_ASAC_ZigBee_fwupd_CC2650_op_read_version;
        	    		p_req->body.CC2650_read_firmware_version.unused = 0xa55a3663;
        				break;
        			}
        			case '1':
        			{
        	    		p_req->ops.CC2650= enum_ASAC_ZigBee_fwupd_CC2650_op_start_update;
        	    		snprintf((char*)p_req->body.CC2650_start_firmware_update.CC2650_fw_signed_filename, sizeof(p_req->body.CC2650_start_firmware_update.CC2650_fw_signed_filename), "%s", "ASACZ_CC2650fw_COORDINATOR.2_6_5");
        				break;
        			}
        			case '2':
        			{
        	    		p_req->ops.CC2650= enum_ASAC_ZigBee_fwupd_CC2650_op_query_update_status;
        	    		p_req->body.CC2650_query_firmware_update_status.unused= 0;
        				break;
        			}
        			case '3':
        			{
        	    		p_req->ops.CC2650= enum_ASAC_ZigBee_fwupd_CC2650_op_query_firmware_file;
        	    		snprintf ((char*)p_req->body.CC2650_query_firmware_file_req.CC2650_fw_query_filename, sizeof(p_req->body.CC2650_query_firmware_file_req.CC2650_fw_query_filename), "%s", "ASACZ_CC2650fw_COORDINATOR.2_6_5");
        				break;
        			}
        			case '4':
        			{
        				p_req->dst.enum_dst = enum_ASAC_ZigBee_fwupd_destination_ASACZ;
        	    		p_req->ops.ASACZ= enum_ASAC_fwupd_ASACZ_op_start_update;
#define def_fw_ASACZ_package "OLINUXINO_ASACZ.0_1_3_build28"
        	    		printf("Using the firmware file: %s\n", def_fw_ASACZ_package);
        	    		snprintf ((char*)p_req->body.ASACZ_do_update_req_body.ASACZ_fw_signed_filename, sizeof(p_req->body.ASACZ_do_update_req_body.ASACZ_fw_signed_filename), "%s", def_fw_ASACZ_package);
        				break;
        			}
        			case '5':
        			{
        				p_req->dst.enum_dst = enum_ASAC_ZigBee_fwupd_destination_ASACZ;
        	    		p_req->ops.ASACZ= enum_ASAC_fwupd_ASACZ_op_status_update;
        				break;
        			}
        		}
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
        					printf("%s: fwupd request error on sendto()", __func__);
        				}
        				else
        				{
        					printf("%s: fwupd request TX OK\n",__func__);
        				}
            		}
        		}
    		}
    	}
#endif

#ifndef ANDROID_NO_CONSOLE
    	// 'N' set get sn
    	if ((c_from_kbd == 'N') || (c_from_kbd == 'n'))
    	{


			printf("SERIAL NUMBER TEST INTERFACE\n");
			printf("0 get, 1 set to 135789, 9 exit\n");
			int option = 0;
			while(1)
			{
				c_from_kbd = getchar();
				if ((c_from_kbd == '0') || (c_from_kbd == '1') )
				{
					option = c_from_kbd;
					break;
				}
				if (c_from_kbd == '9')
				{
					break;
				}

			}
			if (option > 0)
			{
				type_ASAC_Zigbee_interface_request zmessage_tx;
				type_ASAC_ZigBee_interface_administrator_device_info_req * p_req = NULL;
				unsigned int zmessage_size = 0;
				memset(&zmessage_tx, 0, sizeof(zmessage_tx));
				init_header(&zmessage_tx.h, def_signal_strength_req_command_version, enum_ASAC_ZigBee_interface_command_administrator_device_info, get_new_link_id());
				zmessage_size = def_size_ASAC_Zigbee_interface_req((&zmessage_tx), device_info_req);
				p_req = &zmessage_tx.req.device_info_req;
				p_req->op.enum_op = enum_administrator_device_info_op_get_serial_number;
				switch (c_from_kbd)
				{
					case '0':
					default:
					{
						p_req->op.enum_op = enum_administrator_device_info_op_get_serial_number;
						type_administrator_device_info_op_get_serial_number *pget = &p_req->body.get_serial_number;
						memset(pget, 0, sizeof(*pget));
						pget->unused = 0;
						break;
					}
					case '1':
					{
						p_req->op.enum_op = enum_administrator_device_info_op_set_serial_number;
			    		uint32_t new_serial_number = 135789;
			    		char data_crc[64];
						int n_needed_chars = snprintf((char*)data_crc, sizeof(data_crc), "ASACZsn%u", new_serial_number);
						uint32_t key_validate = calcCrcLikeChip((unsigned char*)data_crc, n_needed_chars);
						type_administrator_device_info_op_set_serial_number *pset = &p_req->body.set_serial_number;
						memset(pset, 0, sizeof(*pset));
						pset->key_validate = key_validate;
						pset->new_serial_number = new_serial_number;
						break;
					}
				}
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
							printf("%s: sn request error on sendto()", __func__);
						}
						else
						{
							printf("%s: sn request TX OK\n",__func__);
						}
					}
				}
			}

    	}
#endif

#ifndef ANDROID_NO_CONSOLE
    	// 'G' diagnostic test
    	if ((c_from_kbd == 'G') || (c_from_kbd == 'g'))
    	{

    		printf("DIAGNOSTIC TEST INTERFACE\n");
    		printf("0 start, 1 status, 2 stop 3 system reboot 4 get UTC time 5 restart 9 exit\n");
    		int option = 0;
    		while(1)
    		{
        		c_from_kbd = getchar();
        		if ((c_from_kbd == '0') || (c_from_kbd == '1') || (c_from_kbd == '2')|| (c_from_kbd == '3')|| (c_from_kbd == '4')|| (c_from_kbd == '5'))
        		{
            		option = c_from_kbd;
            		break;
        		}
        		if (c_from_kbd == '9')
        		{
        			break;
        		}

    		}
    		if (option > 0)
    		{
        		type_ASAC_Zigbee_interface_request zmessage_tx;
        		type_ASAC_admin_diag_test_req * p_req = NULL;
        		unsigned int zmessage_size = 0;
        		memset(&zmessage_tx, 0, sizeof(zmessage_tx));
        		init_header(&zmessage_tx.h, def_signal_strength_req_command_version, enum_ASAC_ZigBee_interface_command_administrator_diagnostic_test, get_new_link_id());
        		zmessage_size = def_size_ASAC_Zigbee_interface_req((&zmessage_tx), diag_test_req);
        		p_req = &zmessage_tx.req.diag_test_req;
        		p_req->op.enum_op = enum_admin_diag_test_op_read_status;
        		switch (c_from_kbd)
        		{
        			case '0':
        			{
        	    		p_req->op.enum_op = enum_admin_diag_test_op_start;
        	    		type_admin_diag_test_req_start_body *pstart = &p_req->body.start;
        	    		memset(pstart, 0, sizeof(*pstart));
        	    		pstart->average_type.enum_type = enum_admin_diag_test_average_type_default;
        	    		pstart->batch_acquire_period_ms = 1000;
        	    		pstart->custom_message_length_type = 30;
        	    		pstart->length_type.enum_type = enum_admin_diag_test_message_length_type_default;
        	    		pstart->message_body_type.enum_type = enum_admin_diag_test_message_body_type_default;
        	    		pstart->num_batch_samples_for_average = 50;
        	    		pstart->server_IEEE_address = 0x124B000D7D8002;
        	    		printf("Starting radio diagnostic test with server 0x%"PRIx64"\n", pstart->server_IEEE_address);
        				break;
        			}
        			case '1':
        			{
        	    		p_req->op.enum_op = enum_admin_diag_test_op_read_status;
        	    		type_admin_diag_test_req_status_body *pstatus = &p_req->body.status;
        	    		memset(pstatus, 0, sizeof(*pstatus));
        	    		pstatus->format.enum_format = enum_admin_diag_test_status_req_format_default;
        				break;
        			}
        			case '2':
        			default:
        			{
        	    		p_req->op.enum_op = enum_admin_diag_test_op_stop;
        	    		type_admin_diag_test_req_stop_body *pstop = &p_req->body.stop;
        	    		memset(pstop, 0, sizeof(*pstop));
        	    		pstop->unused = 0;
        				break;
        			}
        			case '3':
        			{
                		init_header(&zmessage_tx.h, def_signal_strength_req_command_version, enum_ASAC_ZigBee_interface_command_administrator_system_reboot, get_new_link_id());
                		zmessage_size = def_size_ASAC_Zigbee_interface_req((&zmessage_tx), system_reboot_req);
                		type_ASAC_ZigBee_interface_administrator_system_reboot_req * p_req_reboot = NULL;
                		p_req_reboot = &zmessage_tx.req.system_reboot_req;
                		p_req_reboot->reboot_req_id = 123456;
                		snprintf((char*)p_req_reboot->reboot_req_message , sizeof(p_req_reboot->reboot_req_message), "%s", "test reboot");
        				break;
        			}
        			case '4':
        			{
                		init_header(&zmessage_tx.h, def_signal_strength_req_command_version, enum_ASAC_ZigBee_interface_command_administrator_UTC, get_new_link_id());
                		zmessage_size = def_size_ASAC_Zigbee_interface_req((&zmessage_tx), UTC_req);
                		type_ASAC_ZigBee_interface_administrator_UTC_req * p_req_UTC = NULL;
                		p_req_UTC = &zmessage_tx.req.UTC_req;
                		p_req_UTC->op.enum_op = enum_UTC_op_get;
        				break;
        			}
        			case '5':
        			{

                		init_header(&zmessage_tx.h, def_administrator_restart_me_command_version, enum_ASAC_ZigBee_interface_command_administrator_restart_me, get_new_link_id());
                		zmessage_size = def_size_ASAC_Zigbee_interface_req((&zmessage_tx), restart_me_req);
                		type_ASAC_ZigBee_interface_administrator_restart_me_req * p_req_restart_me = NULL;
                		p_req_restart_me = &zmessage_tx.req.restart_me_req;
                		p_req_restart_me->restart_req_id = 223344;
                		snprintf((char*)p_req_restart_me->restart_req_message, sizeof(p_req_restart_me->restart_req_message), "%s", "ASAC restart test field");
        				break;
        			}
        		}
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
        					printf("%s: diag request error on sendto()", __func__);
        				}
        				else
        				{
        					printf("%s: diag request TX OK\n",__func__);
        				}
            		}
        		}
    		}
    	}
#endif



#ifndef ANDROID
    	// @ sends user message
    	if (c_from_kbd == '@')
    	{
    		printf("Indirizzo? [1..%i]", numof_available_IEEE_addresses);
    		int address = getchar();
    		while(1)
    		{
        		address = getchar();
        		if (address >= '1' && address <= '9')
        		{
        			break;
        		}
    		}
    		if (address < '0' || address > '9')
    		{
    			address = '1';
    		}
    		address = address -'0';
    		address --;
    		if (address <= 0)
    		{
    			address = 0;
    		}
    		if (address >= (int)numof_available_IEEE_addresses)
    		{
    			address = 0;
    		}


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
        		init_header(&zmessage_tx.h, def_outside_send_message_req_command_version, enum_ASAC_ZigBee_interface_command_outside_send_message, get_new_link_id());
        		unsigned int zmessage_size = def_size_ASAC_Zigbee_interface_req((&zmessage_tx),outside_send_message);
        		type_ASAC_ZigBee_interface_command_outside_send_message_req * p_req = &zmessage_tx.req.outside_send_message;
        		int len = snprintf((char*)p_req->message,sizeof(p_req->message),"%s", buff);
        		type_ASAC_ZigBee_dst_id *pdst = &p_req->dst_id;
    #ifdef ANDROID_NO_CONSOLE
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
        		if (numof_available_IEEE_addresses)
        		{
        			pdst->IEEE_destination_address = available_IEEE_addresses[address];
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



#ifdef ANDROID_NO_CONSOLE
	usleep(1000);
	if (idx_global_loop == 5000)
#else
    	if ((c_from_kbd == 'm') || (c_from_kbd == 'M'))
#endif
    	{
#ifdef ANDROID_NO_CONSOLE
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
    		init_header(&zmessage_tx.h, def_outside_send_message_req_command_version, enum_ASAC_ZigBee_interface_command_outside_send_message, get_new_link_id());
    		unsigned int zmessage_size = def_size_ASAC_Zigbee_interface_req((&zmessage_tx),outside_send_message);
    		type_ASAC_ZigBee_interface_command_outside_send_message_req * p_req = &zmessage_tx.req.outside_send_message;
    		int len = snprintf((char*)p_req->message,sizeof(p_req->message),"%s", text_to_send);
    		type_ASAC_ZigBee_dst_id *pdst = &p_req->dst_id;
#ifdef ANDROID_NO_CONSOLE
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

#ifdef ANDROID_NO_CONSOLE
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
        		init_header(&zmessage_tx.h, def_echo_req_command_version, enum_ASAC_ZigBee_interface_command_network_echo_req, get_new_link_id());
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

#ifndef ANDROID_NO_CONSOLE
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
                		switch(pzmessage_rx->h.c.command_code)
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
                				numof_available_IEEE_addresses = 0;
                				unsigned int i;
                				for (i = 0; i < p_reply->num_devices_in_chunk; i++)
                				{
                					if (i < sizeof(available_IEEE_addresses)/sizeof(available_IEEE_addresses[0]))
                					{
                    					available_IEEE_addresses[i] = p_reply->list_chunk[i].IEEE_address;
                    					numof_available_IEEE_addresses++;
                					}
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
                			case enum_ASAC_ZigBee_interface_command_administrator_diagnostic_test:
                			{
                				type_ASAC_admin_diag_test_reply * p_reply = &pzmessage_rx->reply.diag_test_reply;
                				switch(p_reply->op.enum_op)
                				{
                					case enum_admin_diag_test_op_start:
                					{
                						type_admin_diag_test_reply_start_body * ps = &p_reply->body.start;
                						printf("Start diagnostic test reply received: return code is %u (%s)\n", ps->retcode.uint32_retcode, ps->retcode_ascii);
                						break;
                					}
                					case enum_admin_diag_test_op_read_status:
                					{
                						type_admin_diag_test_reply_status_body * ps = &p_reply->body.status;
                						switch(ps->format.enum_format)
                						{
                							case enum_admin_diag_test_status_req_format_default:
                							{
                								type_admin_diag_test_status_standard_format_body *pf = &ps->body.standard_format_body;
                								printf("#bytes rx OK: %"PRIu64", tx OK %"PRIu64"\n", pf->nbytes_rx_OK, pf->nbytes_tx_OK);
                								printf("#messages rx OK: %"PRIu64", tx OK %"PRIu64"\n", pf->nmsg_rx_OK, pf->nmsg_tx_OK);
                								printf("#messages rx ERR: %"PRIu64", tx ERR %"PRIu64"\n", pf->nmsg_rx_ERR, pf->nmsg_tx_ERR);
                								printf("#bytes per seconds: rx %7.1f, tx %7.1f\n", pf->average_num_bytes_per_second_rx, pf->average_num_bytes_per_second_tx);
                								printf("#messages per seconds: rx %7.1f, tx %7.1f\n", pf->average_num_msg_per_second_rx, pf->average_num_msg_per_second_tx);
                								break;
                							}
                							default:
                							{
                        						printf("Unknown diagnostic test status format received: %u\n", ps->format.uint32_format);
                        						break;
                							}
                						}
                						break;
                					}
                					case enum_admin_diag_test_op_stop:
                					{
                						type_admin_diag_test_reply_stop_body * ps = &p_reply->body.stop;
                						printf("Stop diagnostic test reply received: return code is %u (%s)\n", ps->retcode.uint32_retcode, ps->retcode_ascii);
                						break;
                					}
                					default:
                					{
                						printf("Unknown diagnostic test operation received: %u\n", p_reply->op.uint32_op);
                						break;
                					}
                				}
                				break;
                			}
                			case enum_ASAC_ZigBee_interface_command_administrator_firmware_update:
                			{
                				type_ASAC_ZigBee_interface_command_fwupd_reply * p_reply = &pzmessage_rx->reply.fwupd_reply;
                				printf("firmware update reply:\n");
                				switch(p_reply->dst.enum_dst)
                				{
                					case enum_ASAC_ZigBee_fwupd_destination_ASACZ:
                					{
                						printf("\t from ASACZ\n");
                						switch(p_reply->ops.ASACZ)
                						{
                							case enum_ASAC_fwupd_ASACZ_op_start_update:
                							{
                								printf("op update\n");
                								type_fwupd_ASACZ_do_update_reply_body *p_body = &p_reply->body.ASACZ_do_update_reply_body;
                								printf("\t %s\n", p_body->started_OK ? "started OK": "ERROR");
                								break;
                							}
                							case enum_ASAC_fwupd_ASACZ_op_status_update:
                							{
                								printf("status update\n");
                								type_fwupd_ASACZ_status_update_reply_body *p_body = &p_reply->body.ASACZ_status_update_reply_body;
                								printf("\t percentage %u %% (status %u)\n", p_body->progress_0_100, p_body->u_progress.uint);
                								printf("\t %s: return code %u (%s)\n", p_body->ends_OK ? "OK": (p_body->ends_ERROR ? "ERROR" : "RUNNING"), p_body->result_code, p_body->result_message);
                								break;
                							}
                							default:
                							{
                								printf("invalid op: %u\n", p_reply->ops.uint32_op);
                								break;
                							}
                						}
                						break;
                					}
                					case enum_ASAC_ZigBee_fwupd_destination_CC2650:
                					{
                						printf("\t from CC2650\n");
                						switch(p_reply->ops.CC2650)
                						{
                							case enum_ASAC_ZigBee_fwupd_CC2650_op_read_version:
                							{
                        						printf("\t CC2650 firmware version\n");
                								type_fwupd_CC2650_read_version_reply_body *p_body = &p_reply->body.CC2650_read_firmware_version;
                        						printf("\t\t number: %u.%u.%u (%s)\n", p_body->major, p_body->middle, p_body->minor, p_body->is_valid ? "valid": "*** NOT VALID ***");
                        						printf("\t\t product: %u, transport: %u\n", p_body->product, p_body->transport);
                								break;
                							}
                							case enum_ASAC_ZigBee_fwupd_CC2650_op_query_update_status:
                							{
                        						printf("\t CC2650 query update status\n");
                								type_fwupd_CC2650_query_update_status_reply_body *p_body = &p_reply->body.CC2650_query_firmware_update_status;
                        						printf("\t\t status is %u\n", p_body->status);
                        						if (p_body->ends_OK)
                        						{
                            						printf("\t\t ends OK\n");
                        						}
                        						else if (p_body->ends_ERR)
                        						{
                            						printf("\t\t ends with ERROR !!!\n");
                        						}
                        						printf("\t\t flash write percentage is %u %%\n", p_body->flash_write_percentage);
                        						if (p_body->fw_update_result_code_is_valid)
                        						{
                            						printf("\t\t firmware update result code is %u\n", p_body->fw_update_result_code);
                            						printf("\t\t firmware update result string is %s\n", p_body->fw_update_result_string);
                        						}
                								break;
                							}
                							case enum_ASAC_ZigBee_fwupd_CC2650_op_start_update:
                							{
                        						printf("\t CC2650 start update\n");
                								type_fwupd_CC2650_start_update_reply_body *p_body = &p_reply->body.CC2650_start_firmware_update;
                        						printf("\t\t start result is %s\n", p_body->is_OK ? "OK" : "error");
                        						printf("\t\t error code is %u\n", p_body->result_code);
                        						printf("\t\t error message is %s\n", p_body->result_message);
                								break;
                							}
                							case enum_ASAC_ZigBee_fwupd_CC2650_op_query_firmware_file:
                							{
                        						printf("\t CC2650 query firmware file\n");
                								type_fwupd_CC2650_query_firmware_file_reply *p_body = &p_reply->body.CC2650_query_firmware_file_reply;
                        						printf("\t\t result is %s, extended code %s\n", p_body->retcode == 0 ? "OK" : "error", p_body->query_result_string);
                        						if (p_body->retcode == 0 )
                        						{
                            						printf("\t\t filename is %s\n", p_body->CC2650_fw_query_filename);
                            						printf("\t\t version number is %u.%u.%u\n", p_body->fw_version_major, p_body->fw_version_middle, p_body->fw_version_minor);
                            						printf("\t\t firmware type number is %u\n", p_body->fw_type);
                            						printf("\t\t date string is %s\n", p_body->date);
                            						printf("\t\t version number string is %s\n", p_body->ascii_version_number);
                            						printf("\t\t firmware type string is %s\n", p_body->ascii_fw_type);
                            						printf("\t\t firmware body size is %u bytes\n", p_body->firmware_body_size);
                            						printf("\t\t firmware CRC is 0x%u\n", p_body->firmware_body_CRC32_CC2650);
                            						printf("\t\t magic name is %s\n", p_body->magic_name);
                            						printf("\t\t header CRC is 0x%u\n", p_body->header_CRC32_CC2650);
                        						}
                								break;
                							}
                							default:
                							{
                        						printf("\t unknown operation %u\n", p_reply->ops.CC2650);
                								break;
                							}
                						}
                						break;
                					}
                					default:
                					{
                						printf("unknown dst field\n");
                						break;
                					}
                				}

                				break;
                			}
                			case enum_ASAC_ZigBee_interface_command_administrator_device_info:
                			{
                				type_ASAC_ZigBee_interface_administrator_device_info_reply * p_reply = &pzmessage_rx->reply.device_info_reply;
                				switch(p_reply->op.enum_op)
                				{
                					case enum_administrator_device_info_op_get_serial_number:
									{
                						printf("Get serial number reply\n");
                						type_administrator_device_info_op_get_serial_number_reply *pr = &p_reply->body.get_serial_number;
                						printf("\t serial number = %u (%s)\n", pr->serial_number, pr->is_valid ? "valid": "*** NOT VALID ***");
                						printf("\t return code = %u (%s)\n", pr->retcode.uint_retcode, pr->retcode_ascii);
										break;
									}
                					case enum_administrator_device_info_op_set_serial_number:
									{
                						printf("Set serial number reply\n");
                						type_administrator_device_info_op_set_serial_number_reply *pr = &p_reply->body.set_serial_number;
                						printf("\t serial number = %u (%s)\n", pr->serial_number, pr->is_valid ? "valid": "*** NOT VALID ***");
                						printf("\t return code = %u (%s)\n", pr->retcode.uint_retcode, pr->retcode_ascii);
										break;
									}
                					default:
                					{
                						printf("Unknown device info operation: %u \n", p_reply->op.uint_op);
                						break;
                					}
                				}
                				break;
                			}
                			case enum_ASAC_ZigBee_interface_command_network_probe:
                			{
                				type_ASAC_ZigBee_interface_network_probe_reply * p_reply = &pzmessage_rx->reply.network_probe_reply;
                				switch(p_reply->op.enum_op)
                				{
                					case enum_network_probe_op_discovery:
                					{
                						printf("Discovery probe op received\n");
                						type_ASAC_ZigBee_interface_network_probe_reply_discovery *pd = &p_reply->body.discovery;
                						printf("IEEE Address is:\n");
                						printf("\t 0x%"PRIX64" (%s)\n",pd->IEEE_address_info.IEEE_address, pd->IEEE_address_info.is_valid_IEEE_address ? "valid":"NOT VALID");
                						printf("ASACZ version is:\n");
                						printf("\t %s\n", pd->ASACZ_version.string);
                						printf("\t ver %u.%u.%u\n", pd->ASACZ_version.major_number, pd->ASACZ_version.middle_number, pd->ASACZ_version.minor_number);
                						printf("\t build %u\n", pd->ASACZ_version.build_number);
                						printf("\t date and time %s\n", pd->ASACZ_version.date_and_time);
                						printf("\t notes %s\n", pd->ASACZ_version.notes);
                						printf("\t patch %s\n", pd->ASACZ_version.patch);
                						printf("CC2650 version is:\n");
                						printf("\t number: %u.%u.%u (%s)\n", pd->CC2650_version.major, pd->CC2650_version.middle, pd->CC2650_version.minor, pd->CC2650_version.is_valid ? "valid": "*** NOT VALID ***");
                						printf("\t product: %u, transport: %u\n", pd->CC2650_version.product, pd->CC2650_version.transport);
                						printf("Serial number is:\n");
                						printf("\t %u\n", pd->serial_number);
                						printf("OpenWrt version is:\n");
                						printf("\t %s\n", (char*)pd->OpenWrt_release);
                						printf("Uptime is:\n");
                						printf("\t %"PRIu64" seconds\n", pd->uptime_seconds);
                						break;
                					}
                					default:
                					{
                						printf("Unknown probe op received: %u\n", p_reply->op.uint_op);
                						break;
                					}
                				}
                				break;
                			}
                			case enum_ASAC_ZigBee_interface_command_administrator_system_reboot:
                			{
                				type_ASAC_ZigBee_interface_administrator_system_reboot_reply * p = &pzmessage_rx->reply.system_reboot_reply;
                				printf("System reboot reply received: id = %u, msg = %s\n", p->reboot_req_id, p->reboot_req_message);
                				break;
                			}
                			case enum_ASAC_ZigBee_interface_command_administrator_restart_me:
                			{
                				type_ASAC_ZigBee_interface_administrator_restart_me_reply * p = &pzmessage_rx->reply.restart_me_reply;
                				printf("ASACZ restart reply received: id = %u, msg = %s\n", p->restart_req_id, p->restart_req_message);
                				break;
                			}
                			case enum_ASAC_ZigBee_interface_command_administrator_UTC:
                			{
                				type_ASAC_ZigBee_interface_administrator_UTC_reply * p = &pzmessage_rx->reply.UTC_reply;
                				printf("UTC reply received:\n");
                				printf("\top: %u\n", p->op.uint_op);
                				printf("\treturn code: %u (%s)\n", p->return_code.uint_r, p->return_ascii);
                				time_t t = p->current_system_epoch_time;
                				struct tm * pmytm = gmtime(&t);
                				printf("\tremote epoch time: 0x%"PRIx64", (%s)\n", p->current_system_epoch_time, asctime(pmytm));
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
                				printf("%s: RX unknown message code: %X\n", __func__, pzmessage_rx->h.c.command_code);
                			}
                		}
                	}

                }
        	}
        }
#ifndef ANDROID_NO_CONSOLE
    	usleep(100000);
#endif
    }
    close(sockfd);
    return 0;
}
