//
// Created by Anne Ha on 12/3/23.
//
// TFTP client program - CSS 432 - Winter 2024

#include "TftpCommon.h"

#define SERV_UDP_PORT 61125
#define SERV_HOST_ADDR "127.0.0.1"

/* A pointer to the name of this program for error reporting.      */
char *program;

void handleFirstReceivedPacket(int sockfd, struct sockaddr_in serv_addr, TftpPacketUnion &receivedPkt, socklen_t serLen)
{
    ssize_t rcvFirstBytes = recvfrom(sockfd, &receivedPkt, sizeof(receivedPkt), 0,
                                     (struct sockaddr *)&serv_addr, &serLen);
    if (rcvFirstBytes == -1)
    {
        perror("Error receiving first TFTP packet");
        close(sockfd);
        return;
    }

    // Handle error packet received
    uint16_t opcode = ntohs(receivedPkt.packet.opcode);
    if (opcode == TFTP_ERROR)
    {
        std::cout << "Received TFTP error packet. Error Code: " << ntohs(receivedPkt.errorPacket.errorCode)
                  << ", Error Message: " << receivedPkt.errorPacket.errorMessage << std::endl;
        exit(3);
    }
}

void processRRQ(int sockfd, struct sockaddr_in serv_addr, std::string filePath)
{
    TftpPacketUnion receivedPkt;
    socklen_t serLen = sizeof(serv_addr);

    handleFirstReceivedPacket(sockfd, serv_addr, receivedPkt, serLen);

    std::ofstream file;
    file.open(filePath, std::ios::binary);

    if (!file.good())
    {
        std::cerr << "Error writing to the file." << std::endl;
        return;
    }

    bool isLastPacket = false;
    while (!isLastPacket)
    {
        // Process DATA packet
        uint16_t blockNumber = ntohs(receivedPkt.dataPacket.blockNumber);
        size_t bytesRead = std::min(strlen(receivedPkt.dataPacket.data), static_cast<size_t>(MAX_DATA_LEN));

        // Write the received data to the file
        if (bytesRead < MAX_DATA_LEN)
        {
            isLastPacket = true;
            file.write(receivedPkt.dataPacket.data, bytesRead);
        }
        else
            file.write(receivedPkt.dataPacket.data, MAX_DATA_LEN);

        std::cout << "Received Block #" << blockNumber << std::endl;

        file.flush();

        // Send ACK packet back to the server
        sendAckPacket(sockfd, blockNumber, serv_addr, serLen);

        // Receive the next packet from the server if not last packet
        if (!isLastPacket)
        {
            ssize_t receivedBytes = recvfrom(sockfd, &receivedPkt, sizeof(receivedPkt), 0,
                                             (struct sockaddr *)&serv_addr, &serLen);
            if (receivedBytes == -1)
            {
                perror("Error receiving TFTP data packet");
                close(sockfd);
                return;
            }
        }
    }

    file.close();
}

void processWRQ(int sockfd, struct sockaddr_in serv_addr, std::string filePath)
{
    TftpPacketUnion receivedPkt;
    socklen_t serLen = sizeof(serv_addr);

    handleFirstReceivedPacket(sockfd, serv_addr, receivedPkt, serLen);

    // Open file for read
    std::ifstream file(filePath, std::ios::binary);

    // Handle file open error
    if (!file.is_open())
    {
        std::cerr << "Error opening file for read: " << filePath << std::endl;
        close(sockfd);
        exit(3);
    }

    // Initialize local block number
    uint16_t clientBlockNumber = 0;

    // Wait for the 1st ACK from the server with the block number that the server successfully received
    uint16_t receivedBlockNumber = ntohs(receivedPkt.ackPacket.blockNumber);

    // Check if the acknowledgment has the expected block number
    // If yes, the client knows that the server successfully received the data block
    // If the acknowledgment has a different block number
    // the client may need to retransmit the last data block
    if (receivedBlockNumber != clientBlockNumber)
        std::cerr << "Unexpected ACK received. Expected: " << clientBlockNumber << ", Received: " << receivedBlockNumber << std::endl;

    std::cout << "Received ACK #" << receivedBlockNumber << std::endl;
    // Increment local block number for the next data block
    clientBlockNumber++;

    char buffer[MAX_DATA_LEN];
    bool isLastPacket = false;
    while (!isLastPacket)
    {
        // Create a data packet
        TftpDataPacket dataPacket;
        dataPacket.opcode = htons(TFTP_DATA);
        dataPacket.blockNumber = htons(clientBlockNumber);

        // Read data from the file into buffer and copy to data packet
        file.read(buffer, MAX_DATA_LEN);
        size_t bytesRead = file.gcount();

        if (bytesRead < MAX_DATA_LEN)
            isLastPacket = true; // EOF reached, break the loop

        if (bytesRead != 0)
        {
            for (size_t i = 0; i < bytesRead; i++)
                dataPacket.data[i] = buffer[i];
        }

        // dataPacket.data[bytesRead] = '\0';
        std::cout << bytesRead << dataPacket.data << std::endl;

        // Send data packet to the server
        sendDataPacket(sockfd, dataPacket, clientBlockNumber, serv_addr, serLen);

        // Wait for ACK from the server with the block number that the server successfully received
        if (isLastPacket)
            break;

        TftpAckPacket ackPacket;
        ssize_t ackBytesReceived = receiveAckPacket(sockfd, ackPacket, serv_addr, serLen);
        uint16_t receivedBlockNumber = ntohs(ackPacket.blockNumber);
        std::cout << "Received ACK #" << receivedBlockNumber << std::endl;

        // Check if the acknowledgment has the expected block number
        // If yes, the client knows that the server successfully received the data block
        // If the acknowledgment has a different block number
        // the client may need to retransmit the last data block
        if (receivedBlockNumber != clientBlockNumber)
        {
            std::cerr << "Unexpected ACK received. Expected: " << clientBlockNumber << ", Received: " << receivedBlockNumber << std::endl;
            break;
        }

        // Increment local block number for the next data block
        clientBlockNumber++;
    }

    file.close();
}

/* The main program sets up the local socket for communication     */
/* and the server's address and port (well-known numbers) before   */
/* calling the processFileTransfer main loop.                      */
int main(int argc, char *argv[])
{
    program = argv[0];

    int sockfd;
    struct sockaddr_in cli_addr, serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    memset(&cli_addr, 0, sizeof(cli_addr));

    socklen_t serLen = sizeof(serv_addr);

    // Initialize server and client address structure
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(SERV_HOST_ADDR);
    serv_addr.sin_port = htons(SERV_UDP_PORT);

    cli_addr.sin_family = AF_INET;
    cli_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    cli_addr.sin_port = htons(0);

    // Create a UDP socket
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
    {
        perror("socket creation failed.");
        exit(1);
    }

    // Bind the socket
    if (bind(sockfd, (struct sockaddr *)&cli_addr, sizeof(cli_addr)) < 0)
    {
        perror("bind failed.");
        exit(2);
    }
    std::cout << "Bind socket successful" << std::endl;

    // Verify arguments
    if (argc != 3)
    {
        std::cerr << "Usage: " << argv[0] << " <r|w> <filename>" << std::endl;
        return 0;
    }

    // Parse arguments
    char requestType = argv[1][0];
    char *filename = argv[2];

    if (requestType != 'r' && requestType != 'w')
    {
        std::cerr << "Invalid request type. Use 'r' for read or 'w' for write." << std::endl;
        return 0;
    }

    std::string filePath = std::string(CLIENT_FOLDER) + std::string(filename);

    // Handle read or write request
    std::cout << "Processing TFTP request..." << std::endl;
    if (requestType == 'r')
    {
        handleRequestPacket(filename, TFTP_RRQ, sockfd, serv_addr);
        processRRQ(sockfd, serv_addr, filePath);
    }
    else if (requestType == 'w')
    {
        if (access(filePath.c_str(), F_OK) != 0)
        {
            std::cout << "The file does not exist." << std::endl;
            exit(0);
        }

        handleRequestPacket(filename, TFTP_WRQ, sockfd, serv_addr);
        processWRQ(sockfd, serv_addr, filePath);
    }

    std::cout << "Process finished with exit code 0" << std::endl;

    exit(0);
}
