/*
 * ASAC_ZigBee_network_commands.h
 *
 *  Created on: May 16, 2016
 *      Author: michele
 */

#ifndef ASAC_ZIGBEE_NETWORK_COMMANDS_H_
#define ASAC_ZIGBEE_NETWORK_COMMANDS_H_

#include <stdint.h>


typedef enum
{
// the commands we send to/receive from the outside world, e.g. the user messages
	enum_ASAC_ZigBee_interface_command_outside_first = 0x10000000,
// sends a user message
	enum_ASAC_ZigBee_interface_command_outside_send_message = enum_ASAC_ZigBee_interface_command_outside_first,
// a user message has been received
	enum_ASAC_ZigBee_interface_command_outside_received_message,

// the commands we use to handle the network, e.g. the introduce myself which sets my name in the network
	enum_ASAC_ZigBee_interface_command_network_first = 0x20000000,
// create and announce an input cluster
	enum_ASAC_ZigBee_interface_command_network_input_cluster_register_req = enum_ASAC_ZigBee_interface_command_network_first,
// echo the body
	enum_ASAC_ZigBee_interface_command_network_echo_req,

// the commands used by the administrator
	enum_ASAC_ZigBee_interface_command_administrator_first = 0x30000000,
	enum_ASAC_ZigBee_interface_command_administrator_firmware_update = enum_ASAC_ZigBee_interface_command_administrator_first,
// the command used to reply to unknown commands
	enum_ASAC_ZigBee_interface_command_unknown = 0xFFFFFFFF,

}enum_ASAC_ZigBee_interface_command;

typedef struct _type_ASAC_ZigBee_dst_id
{
	uint64_t IEEE_destination_address; 	// the IEEE destination address; 0 always means the coordinator is the destination
	uint8_t destination_endpoint;		// the destination end-point
	uint16_t cluster_id;				// the cluster (command) identifier
	uint8_t source_endpoint;			// the source end-point
	uint8_t transaction_id;				// the transaction identifier
}__attribute__((__packed__)) type_ASAC_ZigBee_dst_id ;

typedef struct _type_ASAC_ZigBee_src_id
{
	uint64_t IEEE_source_address; 	// the IEEE source address; 0 always means the coordinator is the source
	uint8_t source_endpoint;		// the source end-point
	uint16_t cluster_id;			// the cluster (command) identifier
	uint8_t destination_endpoint;	// the destination end-point
	uint8_t transaction_id;			// the transaction identifier
}__attribute__((__packed__)) type_ASAC_ZigBee_src_id ;

//
//
//
// input cluster register begins here
//
//
//
typedef struct _type_ASAC_ZigBee_interface_network_input_cluster_register_req
{
	uint8_t endpoint;			// the end-point to register
	uint16_t input_cluster_id;	// the input cluster (command) to register
}  __attribute__((__packed__)) type_ASAC_ZigBee_interface_network_input_cluster_register_req;

typedef enum
{
	enum_input_cluster_register_reply_retcode_OK = 0,
	enum_input_cluster_register_reply_retcode_ERR_no_room,
	enum_input_cluster_register_reply_retcode_ERR_invalid_endpoint,
	enum_input_cluster_register_reply_retcode_numof
}enum_input_cluster_register_reply_retcode;

typedef struct _type_ASAC_ZigBee_interface_network_input_cluster_register_reply
{
	uint8_t endpoint;			// the end-point to register
	uint16_t input_cluster_id;	// the input cluster (command) to register
	enum_input_cluster_register_reply_retcode retcode;	// the return code
} __attribute__((__packed__)) type_ASAC_ZigBee_interface_network_input_cluster_register_reply ;

//
//
//
// input cluster register ends here
//
//
//


//
//
//
// echo begins here
//
//
//
#define def_echo_message_max_bytes 32
typedef struct _type_ASAC_ZigBee_interface_network_echo_req
{
	uint8_t message_to_echo[def_echo_message_max_bytes];			// the message to echo
} __attribute__((__packed__)) type_ASAC_ZigBee_interface_network_echo_req;

typedef struct _type_ASAC_ZigBee_interface_network_echo_reply
{
	uint8_t message_to_echo[def_echo_message_max_bytes];			// the message to echo
}__attribute__((__packed__)) type_ASAC_ZigBee_interface_network_echo_reply ;

//
//
//
// echo ends here
//
//
//




//
//
//
// SEND OUTSIDE MESSAGE begins here
//
//
//

// the maximum size for a message to send to / receive from the outside world
#define def_ASAC_ZigBee_interface_max_size_outside_message 128

typedef struct _type_ASAC_ZigBee_interface_command_outside_send_message_req
{
	type_ASAC_ZigBee_dst_id dst_id;		// the destination identifier
	uint8_t message_length;				// the number of bytes in the message
	uint8_t message[def_ASAC_ZigBee_interface_max_size_outside_message];	// the message to send
}__attribute__((__packed__)) type_ASAC_ZigBee_interface_command_outside_send_message_req ;

// the return codes from the send user message outside
typedef enum
{
	enum_ASAC_ZigBee_interface_command_outside_send_message_reply_retcode_OK = 0,
	enum_ASAC_ZigBee_interface_command_outside_send_message_reply_retcode_ERROR_unable_to_push_message, // no room in the radio messages queue
	enum_ASAC_ZigBee_interface_command_outside_send_message_reply_retcode_numof
}enum_ASAC_ZigBee_interface_command_outside_send_message_reply_retcode;

typedef struct _type_ASAC_ZigBee_interface_command_outside_send_message_reply
{
	type_ASAC_ZigBee_dst_id dst_id;		// the destination identifier
	uint8_t message_length;				// the number of bytes in the message
	uint32_t retcode; 	// must be of type enum_ASAC_ZigBee_interface_command_outside_send_message_reply_retcode
	uint32_t id; 		// the message identifier / handle before it leaves the radio
}type_ASAC_ZigBee_interface_command_outside_send_message_reply;

//
//
//
// SEND OUTSIDE MESSAGE ends here
//
//
//


// received message callback begins here
typedef struct _type_ASAC_ZigBee_interface_command_received_message_callback
{
	type_ASAC_ZigBee_src_id src_id;		// the source identifier
	uint8_t message_length;				// the number of bytes in the message
	uint8_t message[def_ASAC_ZigBee_interface_max_size_outside_message];	// the message received
}__attribute__((__packed__)) type_ASAC_ZigBee_interface_command_received_message_callback ;



typedef struct _type_ASAC_ZigBee_interface_unknown_reply
{
	uint32_t the_unknown_code; // the unknown code we got
}type_ASAC_ZigBee_interface_unknown_reply;


//
//
//
// The ZigBee interface command structure begins here
//
//
//

typedef struct _type_ASAC_Zigbee_interface_request
{
	uint32_t code; // the command code, MUST be one of enum_ASAC_ZigBee_interface_command
	// the union of all of the possibles requests from the clients
	union
	{
		// outside messages requests
		type_ASAC_ZigBee_interface_command_outside_send_message_req outside_send_message;

		// network messages requests
		type_ASAC_ZigBee_interface_network_input_cluster_register_req input_cluster_register;
		type_ASAC_ZigBee_interface_network_echo_req echo;

	}req;
}type_ASAC_Zigbee_interface_request;


typedef struct _type_ASAC_Zigbee_interface_command_reply
{
	uint32_t code; // the command code, MUST be one of enum_ASAC_ZigBee_interface_command
	// the union of all of the possibles replies of the server to the clients requests
	union
	{
		// outside messages replies
		type_ASAC_ZigBee_interface_command_outside_send_message_reply outside_send_message;
		// incoming message callback
		type_ASAC_ZigBee_interface_command_received_message_callback received_message_callback;

		// network messages replies
		type_ASAC_ZigBee_interface_network_input_cluster_register_reply input_cluster_register;
		type_ASAC_ZigBee_interface_network_echo_reply echo;

		// reply to an unknown command
		type_ASAC_ZigBee_interface_unknown_reply unknown;
	}reply;
}type_ASAC_Zigbee_interface_command_reply;

#define def_size_ASAC_Zigbee_interface_code(p_the_struct) (sizeof(p_the_struct->code))

#define def_size_ASAC_Zigbee_interface_req(p_the_struct,the_command_body) (sizeof((p_the_struct)->code) + sizeof((p_the_struct)->req.the_command_body))
#define def_size_ASAC_Zigbee_interface_reply(p_the_struct,the_command_body) (sizeof((p_the_struct)->code) + sizeof((p_the_struct)->reply.the_command_body))

//
//
//
// The ZigBee interface command structure ends here
//
//
//

#endif /* ASAC_ZIGBEE_NETWORK_COMMANDS_H_ */
