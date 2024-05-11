#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <argp.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <inttypes.h>
#include <time.h>

/******************************************************************************/
// Server args
struct server_arguments {
	int port;
	int drop;
	_Bool port_check, drop_check;
};

// Structs
struct recv {
	uint32_t seq;
	uint16_t ver;
	uint64_t cSec;
	uint64_t cNano;
} __attribute__((packed));

struct send {
	uint32_t seq;
	uint16_t ver;
	uint64_t cSec;
	uint64_t cNano;
	uint64_t sSec;
	uint64_t sNano;
} __attribute__((packed));

// Client information, per each one
typedef struct client {
	int null;
	int max;
	struct timespec reset;
	struct sockaddr_in addy;
	struct client *next;
} Client;

/******************************************************************************/
// Insert a new client into the existing list
Client* insert(Client **head, struct sockaddr_in client_addr, struct timespec reset_time) {

	Client *curr, *prev = NULL, *to_add = NULL;

	// List is NOT empty, so move curr pointer
	if (head != NULL) {
		curr = *head;

		// Iterate to the end of the list
		while (curr != NULL) {

			// If a matching element is found, exit
			if (curr->addy.sin_addr.s_addr == client_addr.sin_addr.s_addr)
				return curr;
			
			prev = curr;
			curr = curr->next;
		}
	}

	// Allocate memory for the new client
	to_add = malloc(sizeof(*to_add));

	// Check that memory was actually allocated and if so, create new element
	if (to_add != NULL) {
		to_add->null = 0;
					to_add->max = 0;
		to_add->reset = reset_time;
		to_add->addy = client_addr;

		// Actually add the item to the list
		if (prev == NULL)
			*head = to_add;
		else
			prev->next = to_add;
	}
	return to_add;
}

/******************************************************************************/
// Exit function for errors
void exiter(char *str) {
	printf("Failure in %s\n", str);
	exit(EXIT_FAILURE);
}

/******************************************************************************/
// Parse command line arguments
error_t server_parser(int key, char *arg, struct argp_state *state) {
	struct server_arguments *args = state->input;
	error_t ret = 0;
	int i = 0;

	switch(key) {
		// Port
		case 'p':
			/* Validate that port is correct and a number, etc!! */
			for (; i < strlen(arg); i++) {
				if (isdigit(arg[i]) == 0) {
					argp_error(state, "Invalid option for a server port, must be a number");
				}
			}

			if (atoi(arg) <= 1024 || atoi(arg) > 65535) {
				argp_error(state, "Invalid option for a server port!");
			}

			// Assign port
			args->port = atoi(arg);
			args->port_check = 1;
			break;

		// Drop percentage
		case 'd':
			/* Validate that port is correct and a number, etc!! */
			for (; i < strlen(arg); i++) {
				if (isdigit(arg[i]) == 0) {
					argp_error(state, "Drop percentage invalid!");
				}
			}
			// Check that drop in [0,100]
			if (atoi(arg) < 0 || atoi(arg) > 100) {
				argp_error(state, "Drop option not in [0, 100]");
			}

			// Assign drop
			args->drop = atoi(arg);
			args->drop_check = 1;
			break;

		default:
			ret = ARGP_ERR_UNKNOWN;
			break;
	}
	return ret;
}

/******************************************************************************/
// Read in CLI args
struct server_arguments server_parseopt(int argc, char *argv[]) {
	struct server_arguments args;

	/* bzero ensures that "default" parameters are all zeroed out */
	bzero(&args, sizeof(args));

	struct argp_option options[] = {
		{ "port", 'p', "port", 0, "The port to be used for the server" ,0},
		{ "number", 'd', "num", 0, "The num to be used for the server. -1 by default", -1},
		{0}
	};

	// Parses the arguments
	struct argp argp_settings = { options, server_parser, 0, 0, 0, 0, 0 };
	if (argp_parse(&argp_settings, argc, argv, 0, NULL, &args) != 0)
		exiter("server parsing");

	// Validates that a valid port was received
	if (args.port_check == 0)
		exiter("server port check");

	// If there was no drop argument, assign a default of 0
	if (args.drop_check == 0)
		args.drop = 0;

	return args;
}			

/******************************************************************************/
int main(int argc, char *argv[]) {

	int sock, max = 0, reset;
	socklen_t len;
	struct sockaddr_in serv_addr, client_addr;
	struct server_arguments cmd_args;
	struct timespec reset_time, curr_time;
	struct recv from_client;
	struct send to_client;

	/**************************************************************************/
	// Placeholders
	char *curr_name;
	Client *head = NULL, *curr;

	// Grab cmd arguments
	len = sizeof(struct sockaddr_in);
	cmd_args = server_parseopt(argc, argv);

	/**************************************************************************/
	/* Create socket for incoming connections */
	if ((sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
		exiter("creating server in socket");

	/* Construct local address structure */
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(cmd_args.port);

	/* Bind to the local address */
	if (bind(sock, (struct sockaddr *) &serv_addr, len) < 0)
		exiter("failed binding in socket");

	/**************************************************************************/
	while(1) { /* Run forever */

		//Receiving information from the client
		recvfrom(sock, &from_client, sizeof(from_client), 0, (struct sockaddr*) &client_addr, &len);

		// Grab the address for lookup in our table
		curr_name = inet_ntoa(client_addr.sin_addr);

		// Packet gets dropped
		if (cmd_args.drop != 0 && (rand() % (101)) <= cmd_args.drop)
			continue;

		// Packet doesn't get dropped
		else {

			// Grab the initial reset time for the client
			if (clock_gettime(CLOCK_REALTIME, &reset_time) < 0)
				exiter("setting init client time");

			// Insert the client, if it's unique
			curr = insert(&head, client_addr, reset_time);

			// Finding out if the max seq is actually still the max!
			if (ntohl(from_client.seq) < curr->max) {
				printf("%s: %d %d %d\n", curr_name, cmd_args.port, ntohl(from_client.seq), max);
			}
			else {
				curr->max = ntohl(from_client.seq);
			}

			// Grab info for some calculations
			reset = curr->reset.tv_sec;
			max = curr->max;

			/********** TIME RESET STUFF *************************************/
			// Grab curr time
			if (clock_gettime(CLOCK_REALTIME, &curr_time) < 0)
			exiter("getting time in server");

			// For the curr client, check if 2 minutes have elapsed
			if (curr_time.tv_sec - reset > 120) {

				printf("NOT HERE------------------\n");

				if (clock_gettime(CLOCK_REALTIME, &reset_time) < 0)
					exiter("getting time in server");

				curr->max = 0;
				curr->reset = reset_time;
			}
			/********** TIME RESET STUFF *************************************/

			// Sending information back to the client
			to_client.seq = from_client.seq;
			to_client.ver = from_client.ver;
			to_client.cSec = from_client.cSec;
			to_client.cNano = from_client.cNano;
			to_client.sSec = htobe64(curr_time.tv_sec);
			to_client.sNano = htobe64(curr_time.tv_nsec);
			sendto(sock, &to_client, sizeof(to_client), 0, (struct sockaddr*) &client_addr, len);
		}
	}
}
