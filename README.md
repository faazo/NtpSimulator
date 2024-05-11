# NTP Simulator

## Brief

This project consists of a client and server, each to be run separately. This leverages User Datagram Protocols (UDP), which runs atop of IP. However, this protocol provides no guarantees, such as that:
* Not all datagrams will be delivered
* Datagrams may be delivered out of order
* There may be duplicate datagrams delivered

This project simulates the Network Time Protocol (NTP), an IP protocol used to sync device clocks.

## Usage

### Server

Arguments
```
usage: ./server [-p serverPort] [-d dropPercentage]

required parameters:
  -p                The port to bind and listen to

optional parameters:
  -d                The number indicates the percentage chance that the server drops any given UDP payload. This value must be within [0,100)
                        - A value of 0 means the server will not intentionally drop any packets
                        - A value of 100 means the server will intentionally drop all packets 

```
Example call
```
./server -p 4500 -d 15
```

### Client

Arguments
```
usage: ./client [-a targetServerIp] [-p targetServerPort] [-n numRequests] [-t timeoutInSeconds]

required parameters:
  -a                The target server's IP address 
  -p                The target server's listening port
  -n                The number of requests to send to the server. This value must be a base 10 integer and >= 0
  -t                The time in seconds to wait after sending the last request to accept responses
                        - A value of 0 means there is no timeout, and the client will wait indefinitely for a response

```
Example call
```
./client -a 127.0.0.1 -p 4500 -n 100 -t 5
```

## Technical breakdown

### 1. Client Sending Time Requests

The client will send N time request UDP datagrams. Each datagram will include:
* A sequence number (1st datagram is #1, 2nd is #2, etc.)
* The current time (tracked as OriginalClientTimeStamp)

### 2. Server Handling

According to the configured drop percentage, the server will decide if the response will get dropped or not. If not, the server will respond with its clock time (tracked as ServerTimeStamp).

### 3. Client Receives Time Responses

For each time response received, the client will take the current time (tracked as NewClientTimeStamp). It will use this data + the data from the server response to calculate the time offset and round trip delay. These are calculated as follows:
* `offset = [(ServerTimeStamp - OriginalClientTimeStamp) + (ServerTimeStamp - NewClientTimeStamp)] / 2`
* `roundTripDelay = NewClientTimeStamp - OriginalClientTimeStamp`

### 4. Client Prints Output

Once all requests have been sent, the client will begin incrementing its timeout window. If a response is received within the timeout window, the window will refresh. This will continue until either all requests have been responded to, or a timeout is achieved. All requests not delivered by the timeout window's closing are considered dropped.

When it is done, the client will output clock information (in seconds) pertaining to each request and response in the format of either:
* `<SEQUENCE NUMBER>: <OFFSET> <ROUND TRIP DELAY>`
* `<SEQUENCE NUMBER>: Dropped`

## Credits

The contents of the "hash" folder, starter argument parsing code, and details for technical breakdown were provided by instructional staff. The "client.c", "server.c", and "Makefile" files were then developed and written by myself.