#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>

#define PORT 2101
#define BUFFER_SIZE 4096

int fd,sockfd, portno, n;
struct termios tty;
struct sockaddr_in server_addr;
struct hostent *server;
char buffer[BUFFER_SIZE];
// Define the host and the request
char *hst = "eklntrip.escortskubota.com";   
// The HTTP request string must end with a double CRLF ("\r\n\r\n") to signify 
// the end of the headers.
char *get_request ="GET /test2 HTTP/1.1\r\nHost: eklntrip.escortskubota.com\r\nNtrip-Version: Ntrip/2.0\r\nUser-Agent: PythonNTRIP\r\nAuthorization: Basic YWRtaW46UEFOQGJ4OTky\r\nConnection: keep-alive\r\n\r\n";

void string_bytes(char strg [],size_t len);
void error(const char *msg) {
    perror(msg);
    exit(1);
}
void WPortInit();
void NtripSocketInit(char *host,char *httpreq);
int main(int argc, char *argv[]) {
    
    /*---------- SERIAL PORT --------*/
    const char *portname = "/dev/ttyUSB0"; // Change to your serial port (e.g., /dev/ttyS0, /dev/ttyAMA0)

    // 1. Open the serial port
    fd = open(portname, O_RDWR | O_NOCTTY | O_SYNC);
    if (fd < 0) {
        perror("Error opening serial port");
    }

    WPortInit();
    NtripSocketInit(hst,get_request);

    int k=0;
    while ((n = read(sockfd, buffer, BUFFER_SIZE - 1)) > 0) {
    unsigned char* bytes = (unsigned char*)malloc(n);
    if (bytes == NULL) {
        perror("Failed to allocate memory");
        return 1;
    }
    k+=1;
    // Copy the string data to the byte array
    memcpy(bytes,buffer,n);

    // Print the byte array in hexadecimal format
    // printf("Byte array size: %d and iterations:%d\n",n,k);
    
    ssize_t z = write(fd,bytes, n); // Exclude null terminator

	if (z < 0) {
    		perror("Error writing to serial port");
	} else {
    		printf("Wrote %zd bytes\n", z);
	}
    
   /* for (size_t i = 0; i < n; i++) {
        
        printf("%02x ", bytes[i]);
    }
    printf("\n");*/

    // Free the allocated memory
    free(bytes);
    bzero(buffer, BUFFER_SIZE);
    }
    if (n < 0) 
        error("ERROR reading from socket");

    // 6. Close the socket
    close(sockfd);
    close(fd);
    return 0;
}


void WPortInit(){


    // 2. Get current terminal attributes
    if (tcgetattr(fd, &tty) != 0) {
        perror("Error from tcgetattr");
        
    }

    // 3. Configure the port for non-canonical mode and raw data
    cfsetospeed(&tty, B115200); // Set output baud rate (e.g., B9600)
    cfsetispeed(&tty, B115200); // Set input baud rate

    // Disable canonical mode (ICANON) and other input processing
    tty.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
    // Disable output processing (OPOST)
    tty.c_oflag &= ~(OPOST);
    // Set 8-bit characters (CS8) and disable parity (PARENB)
    tty.c_cflag |= (CS8);
    tty.c_cflag &= ~(CSIZE | PARENB);

    // Set VMIN and VTIME for non-blocking read behavior (optional, primarily for reading)
    // MIN=0, TIME=0 makes read() return immediately with available chars or 0/EAGAIN
    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 0;

    // 4. Apply the attributes
    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        perror("Error from tcsetattr");
    }

}

void NtripSocketInit(char *host,char *httpreq){

    // 1. Create a socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) 
        error("ERROR opening socket");

    // 2. Get server details
    server = gethostbyname(host);
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host\n");
        exit(1);
    }
    
    bzero((char *) &server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, 
         (char *)&server_addr.sin_addr.s_addr,
         server->h_length);
    server_addr.sin_port = htons(PORT);

    // 3. Connect to the server
    if (connect(sockfd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) 
        error("ERROR connecting");

    // 4. Send the HTTP request
    n = write(sockfd, get_request, strlen(httpreq));
    if (n < 0) 
        error("ERROR writing to socket");
    
    printf("Sent HTTP Request:\n%s\n", httpreq);
    printf("--- Server Response ---\n");

    // 5. Read the server response (which includes HTTP response headers and body)
    bzero(buffer, BUFFER_SIZE);

}



void string_bytes(char strg [],size_t len){

for( int k=0; k<len; k+=1){
       char inp = strg[k];
       unsigned char val = (unsigned char)inp;
    printf("byte value (hex): %x\n",val);
}

}
