#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

#define PORT 2101
#define BUFFER_SIZE 4096

int main() {
    int sock = 0;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE] = {0};
    // The HTTP GET request message, including headers and the mandatory CRLF CRLF ending
    char *hello = "GET /test2 HTTP/1.1\r\nHost: eklntrip.escortskubota.com\r\nNtrip-Version: Ntrip/2.0\r\nUser-Agent: PythonNTRIP\r\nAuthorization: Basic YWRtaW46UEFOQGJ4OTky\r\nConnection: close\r\n\r\n";

    // 1. Create socket file descriptor
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    // Convert IPv4 and IPv6 addresses from text to binary form
    if (inet_pton(AF_INET, "http://eklntrip.escortskubota.com", &serv_addr.sin_addr) <= 0) {
        perror("Invalid address/ Address not supported");
        exit(EXIT_FAILURE);
    }

    // 2. Connect to the server
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connection Failed");
        exit(EXIT_FAILURE);
    }

    // 3. Send the HTTP request
    send(sock, hello, strlen(hello), 0);
    printf("HTTP Request sent:\n%s", hello);

    // 4. Read the response from the server
    printf("\n---- SERVER RESPONSE HEADERS AND BODY ----\n");
    ssize_t bytes_read;
    while ((bytes_read = recv(sock, buffer, BUFFER_SIZE - 1, 0)) > 0) {
        buffer[bytes_read] = '\0'; // Null-terminate the received data
        printf("%s", buffer);
        memset(buffer, 0, BUFFER_SIZE); // Clear buffer for next read
    }

    // 5. Close the socket
    close(sock);

    return 0;
}
