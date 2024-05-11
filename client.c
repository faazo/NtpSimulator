#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <argp.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <inttypes.h>
#include <time.h>
#include <math.h>

/******************************************************************************/

struct client_arguments {
	struct sockaddr_in info; //Stores IP address + port
	int num;
	int timeout;
	int ip_check, port_check, num_check, time_check;
};

/******************************************************************************/

struct time_keep {
	int received;
	float delta;
	float theta;
};

struct to_send {
	uint32_t seq;
	uint16_t ver;
	uint64_t cSec;
	uint64_t cNano;
} __attribute__((packed));

struct to_recv {
	uint32_t seq;
	uint16_t ver;
	uint64_t cSec;
	uint64_t cNano;
	uint64_t sSec;
	uint64_t sNano;
} __attribute__((packed));

/******************************************************************************/
// Exit function
void exiter(char *str) {
	printf("Failure in %s\n", str);
	exit(EXIT_FAILURE);
}

/******************************************************************************/
// Function for validating that the argument is only ints
int int_check(char *arg, struct argp_state *state) {
	int i = 0;
	/* validate arg */
	for (; i < strlen(arg); i++) {
		if (isdigit(arg[i]) == 0) {
			argp_error(state, "Invalid input");
		}
	}
	return 1;
}

/******************************************************************************/
// Parse cmd line args
error_t client_parser(int key, char *arg, struct argp_state *state) {
	struct client_arguments *args = state->input;
	error_t ret = 0;

	switch(key) {
		case 'a':
			args->info.sin_family = AF_INET;
			args->info.sin_addr.s_addr = inet_addr(arg);
			args->ip_check = 1;
		break;

		case 'p':
			int_check(arg, state);
			args->info.sin_port = htons(atoi(arg));
			args->port_check = 1;
		break;

		case 'n':
			if (!int_check(arg, state) || atoi(arg) < 0) {
				argp_error(state, "Invalid option for num of time requests.\n");
			}

			args->num = atoi(arg);
			args->num_check = 1;
		break;

		case 't':
			if (!int_check(arg, state) || atoi(arg) < 0) {
				argp_error(state, "Invalid option for a timeout\n");
			}

			args->timeout = atoi(arg);
			args->time_check = 1;
		break;

		default:
			ret = ARGP_ERR_UNKNOWN;
		break;
	}
	return ret;
}

/******************************************************************************/
struct client_arguments client_parseopt(int argc, char *argv[]) {
	struct argp_option options[] = {
		{ "addr", 'a', "addr", 0, "The IP address the server is listening at", 0},
		{ "port", 'p', "port", 0, "The port that is being used at the server", 0},
		{ "num", 'n', "timeRequests", 0, "The number of time requests the client will send the server", 0},
		{ "timeout", 't', "timeout", 0, "The time in seconds a client will wait after sending its last timereq to receive a response", 0},
		{0}
	};

	struct argp argp_settings = { options, client_parser, 0, 0, 0, 0, 0 };

	struct client_arguments args;
	bzero(&args, sizeof(args));

	if (argp_parse(&argp_settings, argc, argv, 0, NULL, &args) != 0)
		exiter("parse");

	/* Validates that all input that was needed, was received. */
	if (args.ip_check == 0 || args.port_check == 0 || args.num_check == 0 || args.time_check == 0)
		exiter("input checking");

	return args;
	}

/******************************************************************************/
int main(int argc, char *argv[]) {

	int sock, counter = 0, i, goal, ret;
	socklen_t len;
	struct client_arguments ele;
	struct sockaddr_in serv_addr;
	struct timespec time0, time2;
	struct time_keep *cool_name;
	struct to_send to_server;
	struct to_recv from_server;
	struct timeval timeout;

/******************************************************************************/
	//Parse options as the client would
	len = sizeof(serv_addr);
	ele = client_parseopt(argc, argv);
	cool_name = malloc((ele.num + 1) * (sizeof *cool_name));
	timeout.tv_sec = ele.timeout;
	timeout.tv_usec = 0;

/*****************************************************************************/
	//Create a socket
	if ((sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
		exiter("socket creation on client");

	//Construct the server address structure
	bzero(&serv_addr, len);
	serv_addr = ele.info;

/*****************************************************************************/
	//Sending out all requests
	while (counter < ele.num) {
		if (clock_gettime(CLOCK_REALTIME, &time0) < 0)
			exiter("getting time");

		to_server.seq = htonl(counter + 1);
		to_server.ver = htons(7);
		to_server.cSec = htobe64(time0.tv_sec);
		to_server.cNano = htobe64(time0.tv_nsec);

		sendto(sock, &to_server, sizeof(to_server), 0, (struct sockaddr*) &serv_addr, len);
		counter++;
	}

/*****************************************************************************/
	counter = 0;
	goal = setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(struct timeval));

/*****************************************************************************/
	//Receiving the responses!!
	while (counter < ele.num) {

		//Receiving information from the client!
		ret = recvfrom(sock, &from_server, sizeof(from_server), 0, (struct sockaddr*) &serv_addr, &len);
		if (ret < goal || errno == EAGAIN || errno == EWOULDBLOCK)
			break;

		//Grabbing T2
		if (clock_gettime(CLOCK_REALTIME, &time2) < 0)
			exiter("getting time");

		//This code allows for duplicated packets to update the value to the most recent one
		//Passing received info on to the printing data structure
		cool_name[ntohl(from_server.seq)].received = 1;

		//Calculating theta
		cool_name[ntohl(from_server.seq)].theta =
			((be64toh(from_server.sSec) + be64toh(from_server.sNano)/pow(10, 9)) -
			(be64toh(from_server.cSec) + be64toh(from_server.cNano)/pow(10, 9)) +
			(be64toh(from_server.sSec) + be64toh(from_server.sNano)/pow(10, 9)) -
			(time2.tv_sec + (time2.tv_nsec/pow(10, 9)))) / 2;

		//Calculating Delta
		cool_name[ntohl(from_server.seq)].delta = (time2.tv_sec + (time2.tv_nsec/pow(10, 9))) - (be64toh(from_server.cSec) + (be64toh(from_server.cNano)/pow(10, 9)));
		counter++;
	}

/*****************************************************************************/
	//Client output
	for (i = 1; i <= ele.num; i++) {
		if (cool_name[i].received == 1)
			printf("%d: %0.4f %0.4f\n", i, cool_name[i].theta, cool_name[i].delta);
		else
			printf("%d: Dropped\n", i);
	}

/*****************************************************************************/
	free(cool_name);
	close(sock);
	return 0;
}
