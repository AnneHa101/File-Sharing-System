//
// Created by Anne Ha on 12/3/23.
//
// TFTP server program over UDP - CSS432 - winter 2024

#include "TftpCommon.h"

#define SERV_UDP_PORT 61125

char *program;

int handleIncomingRequest(int sockfd)
{
    struct sockaddr_in cli_addr;

    size_t receivedBytes;
    socklen_t cliLen;
    char mesg[MAX_PACKET_LEN];

    for (;;)
    {
        std::cout << "\nWaiting to receive request\n"
                  << std::endl;

        // Receive the 1st request packet from the client
        cliLen = sizeof(cli_addr);
        receivedBytes = recvfrom(sockfd, mesg, MAX_PACKET_LEN, 0, (struct sockaddr *)&cli_addr, &cliLen);
        if (receivedBytes < 0)
        {
            perror("Error receiving request packet");
            continue; // continue listening
        }

        // Parse the request packet
        TftpPacketUnion receivedPkt;
        memcpy(&receivedPkt, mesg, sizeof(TftpPacketUnion));
        uint16_t opcode = ntohs(receivedPkt.dataPacket.opcode);

        if (opcode < TFTP_RRQ || opcode > TFTP_ERROR)
        {
            std::cout << "Received message has an illegal opcode." << std::endl;
            handleErrorPacket(TFTP_ERROR_ILLEGAL_OPERATION, "Illegal opcode", sockfd, cli_addr, cliLen);
            exit(4);
        }

        const char *filename = reinterpret_cast<char *>(receivedPkt.requestPacket.filename);
        std::cout << "Requested filename is: " << filename << std::endl;

        std::string filePath = std::string(SERVER_FOLDER) + std::string(filename);

        if (opcode == TFTP_RRQ)
        {
            // Handle error code 1: file does not exist on server
            if (access(filePath.c_str(), F_OK) != 0)
            {
                std::cout << "The file does not exist." << std::endl;
                handleErrorPacket(TFTP_ERROR_FILE_NOT_FOUND, "File does not exist", sockfd, cli_addr, cliLen);
                continue;
            }

            // Open file for read
            std::ifstream file(filePath, std::ios::binary);

            // Handle file not found error
            if (!file.is_open())
            {
                handleErrorPacket(TFTP_ERROR_FILE_NOT_FOUND, "File not found", sockfd, cli_addr, cliLen);
                // Continue to the next iteration of the loop
                continue;
            }

            // File found, proceed with sending data packets to the client

            // Initialize local block number
            uint16_t serverBlockNumber = 1;
            bool isLastPacket = false;

            while (!isLastPacket)
            {
                // Create a data packet
                TftpDataPacket dataPacket;
                dataPacket.opcode = htons(TFTP_DATA);

                // Read data from the file into buffer and copy to data packet
                char buffer[MAX_DATA_LEN];
                file.read(buffer, MAX_DATA_LEN);
                size_t bytesRead = file.gcount();

                if (bytesRead < MAX_DATA_LEN)
                    isLastPacket = true; // EOF reached, break the loop

                if (bytesRead != 0)
                {
                    for (size_t i = 0; i < bytesRead; i++)
                        dataPacket.data[i] = buffer[i];
                }

                for (int attempt = retryCount; attempt <= MAX_RETRY_COUNT; attempt++)
                {
                    try
                    {
                        // Send data packet to the client
                        sendDataPacket(sockfd, dataPacket, serverBlockNumber, cli_addr, cliLen);

                        // Start the timer and set for 1 sec
                        registerTimeoutHandler();
                        alarm(1);

                        // Wait for ACK from the client with the block number that the client successfully received
                        TftpAckPacket ackPacket;
                        ssize_t bytesReceived = recvfrom(sockfd, &ackPacket, sizeof(TftpAckPacket), 0, (struct sockaddr *)&cli_addr, &cliLen);
                        uint16_t receivedBlockNumber = ntohs(ackPacket.blockNumber);

                        if (receivedBlockNumber != serverBlockNumber)
                        {
                            std::cerr << "Unexpected ACK received. Expected: " << serverBlockNumber << ", Received: " << receivedBlockNumber << std::endl;
                            break;
                        }

                        // Clear the timer after receiving the packet and reset retry count
                        alarm(0);
                        retryCount = 0;

                        std::cout << "Received ACK #" << receivedBlockNumber << std::endl;
                        // Increment local block number for the next data block
                        serverBlockNumber++;
                        break;
                    }
                    catch (const std::exception &e)
                    {
                        std::cerr << e.what() << '\n';

                        if (attempt < MAX_RETRY_COUNT)
                        {
                            std::cout << "Retransmitting #" << serverBlockNumber << std::endl;
                            std::this_thread::sleep_for(std::chrono::milliseconds(200));
                        }
                    }
                }
            }

            // Close the file after sending all data
            file.close();
        }
        else if (opcode == TFTP_WRQ)
        {
            // Handle error code 6: file already exists on server
            if (access(filePath.c_str(), F_OK) == 0)
            {
                std::cout << "The file already exists." << std::endl;
                handleErrorPacket(TFTP_ERROR_FILE_EXISTS, "File already exists", sockfd, cli_addr, cliLen);
                continue;
            }

            // Open file for write
            std::ofstream file;
            file.open(filePath, std::ios::binary);

            // Handle file open error
            if (!file.good())
            {
                handleErrorPacket(TFTP_ERROR_ACCESS_VIOLATION, "Unable to open file for write", sockfd, cli_addr, cliLen);
                // Continue to the next iteration of the loop
                continue;
            }

            // File open successful, proceed with receiving data packets from the client

            // Initialize local block number
            uint16_t serverBlockNumber = 0;

            // Acknowledge the WRQ by sending an initial ACK with block number 0
            sendAckPacket(sockfd, serverBlockNumber, cli_addr, cliLen);
            serverBlockNumber++;

            char buffer[MAX_DATA_LEN];
            bool isLastPacket = false;
            if (registerTimeoutHandler() == -1)
                exit(1);

            // Receive data packets and write data to the file
            while (!isLastPacket)
            {
                // Start the timer and set for 1 sec
                alarm(1);

                // Receive data packet from the client
                TftpDataPacket dataPacket;
                ssize_t bytesReceived = recvfrom(sockfd, &dataPacket, sizeof(TftpDataPacket), 0, (struct sockaddr *)&cli_addr, &cliLen);

                if (bytesReceived < 0)
                {
                    if (errno == EINTR)
                    {
                        printf("Receive timed out\n");
                        alarm(0);

                        if (retryCount < MAX_RETRY_COUNT)
                        {
                            printf("Retransmitting packet...\n");
                            sendAckPacket(sockfd, serverBlockNumber, cli_addr, cliLen);
                        }
                        else
                        {
                            printf("Transmission aborted\n");
                            retryCount = 0; // Reset retry count for next attempt
                            break;
                        }
                    }
                    else
                    {
                        perror("recvfrom failed");
                        break;
                    }
                }
                else if (bytesReceived > 0)
                {
                    alarm(0);       // Cancel timer
                    retryCount = 0; // Reset retry count for next attempt
                    uint16_t receivedBlockNumber = ntohs(dataPacket.blockNumber);

                    // Check if the acknowledgment has the expected block number
                    if (receivedBlockNumber == serverBlockNumber)
                    {
                        std::cout << strlen(dataPacket.data) << std::endl;
                        if (strlen(dataPacket.data) == 0)
                            break;

                        std::cout << "Received block #" << receivedBlockNumber << std::endl;
                        memcpy(buffer, dataPacket.data, MAX_DATA_LEN);

                        // Write data to the file
                        if (strlen(dataPacket.data) == MAX_DATA_LEN + 1)
                            file.write(dataPacket.data, MAX_DATA_LEN);
                        else
                        {
                            file.write(dataPacket.data, strlen(dataPacket.data));
                            isLastPacket = true;
                        }
                        file.flush();

                        // Acknowledge the received data block
                        sendAckPacket(sockfd, serverBlockNumber, cli_addr, cliLen);

                        // Check if it is the last data block
                        if (strlen(dataPacket.data) <= MAX_DATA_LEN)
                        {
                            isLastPacket = true;
                            break;
                        }

                        // Increment local block number for the next ACK
                        serverBlockNumber++;
                    }
                    else
                    {
                        std::cerr << "Unexpected block number received." << receivedBlockNumber << serverBlockNumber << std::endl;
                        // Send error packet to the client
                        handleErrorPacket(TFTP_ERROR_NOT_DEFINED, "Unexpected block number received", sockfd, cli_addr, cliLen);
                        break;
                    }
                }
            }

            // Close the file after receiving all data
            file.close();
        }
    }
}

int main(int argc, char *argv[])
{
    program = argv[0];

    int sockfd;
    struct sockaddr_in serv_addr;

    memset(&serv_addr, 0, sizeof(serv_addr));

    // Initialize the server address
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(SERV_UDP_PORT);

    // Create UDP socket
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
    {
        perror("socket creation failed.");
        exit(EXIT_FAILURE);
    }

    // Bind the socket
    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        perror("bind failed.");
        exit(EXIT_FAILURE);
    }

    handleIncomingRequest(sockfd);

    close(sockfd);
    return 0;
}