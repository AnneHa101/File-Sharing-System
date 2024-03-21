# File-Sharing-System

TRANSFER FILES USING TFTP PROTOCOL

The client/server network application written in C/C++ on a UNIX platform enables server and client to transfer files between each other over TFTP protocol.

The program implement a subset of the Trivial File Transfer Protocol, Revision 2, as described in RFC 1350. TFTP is used over local area networks with low error rates, low delays, and high speeds. It employs a simple stop-and-wait scheme over UDP. 

In a TFTP file transfer there are two parties involved: a client and a server. The client may read a file from the server, or write a file to the server.

The server runs continuously, listening on a predefined UDP port. If a received client request can be satisfied, a file transfer session begins and ends either after successful completion, or after a fatal error occurs. While the clients exit after each session terminates, the server must remain running to listen for the next client request, if it has not been terminated due to a fatal error.

To test your programs, you may start your server in one terminal and your clients in another terminal. Your client takes two arguments. The first indicates whether it is a read or write request, the second is the filename to be read or written. Eg: tftpclient r filename or tftpclient w filename, to read or write files respectively. A read request from the client will download the file from the server, while a write request from the client will upload the file to the server.

During normal protocol operation, your programs send and receive messages according to protocol rules. The core logic of the program consists of a main receive/send loop plus some initialization and termination code. In the loop, you wait for a message from the peer. If it is the expected message, you send your next message or terminate the session successfully. You need to know the message number (data block #) that you are next expecting to see, so that you can distinguish between new and duplicate messages, whether DATA or ACK.

# Important things to note
1. tftp-client should only take two arguments. The first argument should be either exactly “r” (for RRQ) or exactly “w” (for WRQ). The second argument should be the filename only, without including any path to the file. tftp-server should not take any arguments.
2. tftp-server should only read/write files from/to the folder with the exact name “server-files”. tftp-client should only read/write files from/to the folder with the exact name “client-files”.
3. Only the octet file transfer mode will be implemented, not the netascii or mail modes mentioned in RFC 1350. As a result, no data conversion is required at the receiving end.
4. The program also handles timeout, retransmission, or error conditions.
5. After a file transfer is complete, your server must remain running and be able to handle the next file transfer requests.
6. Must support both text file and binary file transfer

# About numeric packet fields
All numeric fields are unsigned short integers (2 bytes) in network byte order, which means that multibyte numbers are stored with the high byte first. Adopting this convention ensures compatibility between systems with different byte ordering. To convert a number from the host byte order, whatever this order is, to network byte order, you use htonl() for long integers and htons() for short integers. ntohl() and ntohs() perform the reverse conversion. By always performing host to network conversion before storing a number and network to host conversion after reading it, your program will work regardless of the platform used. The numeric fields (packet type, block number and error code) are in fixed offsets from the beginning of the packet. To read from or store to them, get a pointer to the beginning of the packet, add (if required) the correct number of bytes to it, and convert it to the correct pointer type before dereferencing it. IP and UDP numeric fields are also in network byte order and unsigned.

# About handling timeout and restransmission
According to TFTP RFC: If a packet gets lost in the network, the intended recipient will timeout and may retransmit his last packet (which may be data or an acknowledgment), thus causing the sender of the lost packet to retransmit that lost packet. The sender has to keep just one packet on hand for retransmission, since the lock step acknowledgment guarantees that all older packets have been received.  Notice that both machines involved in a transfer are considered senders and receivers. One sends data and receives acknowledgments, the other sends acknowledgments and receives data.

In this program, the timeout and retransmission is handled on the server side only using a timeout of 1 seconds, and a maximum retransmit of 10 times.
• Start a timer of 1s before calling recvfrom. When recvfrom is called, the server will block on that call until it receives some packet from the network. 
• If within 1s, the server received something, it should reset the timer.
• If within 1s, the server does not receive anything, a timeout event will occur, which will interrupt the recvfrom system call. In that case, recvfrom will return -1, and errno EINTR will be set. This is how the server knows that a timeout has occurred. See recvfrom() man page.
• The timeout may be then handled by either retransmitting the last packet (Data or ACK) or abort the transmission if it has been already retransmitted for 10 times. In case of abort, the server should remain running to wait for the next request.

# Command used for testing
g++ -std=c++11 TftpServer.cpp TftpCommon.cpp -o tftp-server
./tftp-server

g++ -std=c++11 TftpClient.cpp TfTpCommon.cpp -o tftp-client 
./tftp-client w client-to-server-large.txt
./tftp-client r server-to-client-large.txt


