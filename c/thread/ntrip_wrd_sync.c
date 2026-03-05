#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#include <netdb.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>

#define BAUDRATE B115200
#define BUF 255
#define PORT 2101
#define BUFFER_SIZE 4096
#define BUF_SIZE 256

int fdg,sockfd, portno, n;
struct termios tty;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
char buffer[BUFFER_SIZE];

// Shared resources
char serial_buffer[BUF_SIZE];
size_t bytes_read = 0;
pthread_mutex_t buffer_mutex = PTHREAD_MUTEX_INITIALIZER;
int serial_fd = -1; // File descriptor for the serial port
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

// Define the host and the request
char *hst = "eklntrip.escortskubota.com";   
// The HTTP request string must end with a double CRLF ("\r\n\r\n") to signify 
// the end of the headers.
char *get_request ="GET /test2 HTTP/1.1\r\nHost: eklntrip.escortskubota.com\r\nNtrip-Version: Ntrip/2.0\r\nUser-Agent: PythonNTRIP\r\nAuthorization: Basic YWRtaW46UEFOQGJ4OTky\r\nConnection: keep-alive\r\n\r\n";

void WPortInit();
void NtripSocketInit(char *host,char *httpreq);
void configure_serial_port(int fd);
void *ntrip_dev(void *arg){
    const char *portname = "/dev/ttyUSB0"; // Change to your serial port (e.g., /dev/ttyS0, /dev/ttyAMA0)

 fdg = open(portname, O_RDWR | O_NOCTTY | O_SYNC);
    if (fdg < 0) {
        perror("Error opening serial port----- Write");
    }
    WPortInit(); 

    NtripSocketInit(hst,get_request);
    
    while (1){
    pthread_mutex_lock(&lock);
    n = read(sockfd, buffer, BUFFER_SIZE - 1);
     unsigned char* bytes = (unsigned char*)malloc(n);
    if (bytes == NULL) {
        perror("Failed to allocate memory");
    }
    // Copy the string data to the byte array
    memcpy(bytes,buffer,n);
     ssize_t z = write(fdg,bytes, n); // Exclude null terminator

	if (z < 0) {
    		perror("Error writing to serial port");
	} else {
    		printf("Wrote %zd bytes\n", z);
	}
    free(bytes); 
    // Free the allocated memory
    
    bzero(buffer, BUFFER_SIZE);
    pthread_mutex_unlock(&lock);
    }
    if (n < 0) 
        perror("ERROR reading from socket");
         close(sockfd);
    close(sockfd);
    pthread_exit(NULL);

}

// The thread function that reads from the serial port
void *serial_reader_thread(void *arg) {
    // int fd = *((int*)arg); // If passing fd directly as a pointer
    // Use the global serial_fd instead for simplicity
    const char* serial_port_path = "/dev/ttyUSB0"; // Replace with your port (e.g., /dev/ttyUSB0, /dev/ttyACM0)

    // Open the serial port
    serial_fd = open(serial_port_path, O_RDWR | O_NOCTTY | O_SYNC);
    if (serial_fd < 0) {
        perror(" READ Error opening serial port");
        
    }
    
    configure_serial_port(serial_fd);
    while (1) {
        // Read data from the serial port
        pthread_mutex_lock(&buffer_mutex);
        int count = read(serial_fd, serial_buffer, sizeof(serial_buffer) - 1);
        
        if (count > 0) {
            serial_buffer[count] = '\0'; // Null-terminate for string handling
            printf("Data received: %s\\n", serial_buffer);
            // Protect shared buffer with a mutex before writing
            
            
        } else if (count < 0) {
            perror("Error reading from serial port");
            break; // Exit loop on error
        }
        pthread_mutex_unlock(&buffer_mutex);
    }
    pthread_exit(NULL);
}



int main(){
pthread_t tid, wid;
    if (pthread_mutex_init(&lock, NULL) != 0) {
        fprintf(stderr, "Mutex init failed\n");
        return 1;
    }


    // Create the reader thread
    if (pthread_create(&tid, NULL,ntrip_dev, NULL) != 0) {
        fprintf(stderr, "Error creating thread\\n");
        close(sockfd);
    }
    
     // Create the reader thread
    if (pthread_create(&wid, NULL, serial_reader_thread, NULL) != 0) {
        fprintf(stderr, "READING Error creating thread\\n");
        close(serial_fd);
        return 1;
    }

    // Clean up (in a real app you'd loop and manage termination)
    pthread_join(tid, NULL); // Wait for the reader thread to finish
    // Clean up (in a real app you'd loop and manage termination)
    pthread_join(wid, NULL); // Wait for the reader thread to finish
    close(sockfd);
    close(serial_fd);
    close(fdg);
    pthread_mutex_destroy(&buffer_mutex);
    pthread_mutex_destroy(&lock);
 return 0;

}

// Function to configure the serial port
void configure_serial_port(int fd) {
    struct termios oldtio, newtio;
        // Set baud rate, enable receiver, set canonical mode, etc.
    // Example: cfsetospeed(&tty, B9600); cfsetispeed(&tty, B9600);

    if (tcgetattr(fd, &oldtio) != 0) {
        perror("Error from tcgetattr");
        return;
    }

   // tcgetattr(fd, &oldtio);
    // Copy settings to a new struct and modify
    newtio = oldtio; 

    // 3. Configure the serial port for canonical mode
    // c_cflag: Control modes
    newtio.c_cflag |= (CREAD | CLOCAL); // Enable receiver, ignore modem control lines
    // CSIZE: Character size mask - must be cleared before setting
    newtio.c_cflag &= ~CSIZE; 
    newtio.c_cflag |= CS8;              // 8 data bits

    // c_lflag: Local modes
    newtio.c_lflag |= ICANON;           // Enable canonical input mode
    newtio.c_lflag &= ~ECHO;            // Disable echo (optional)
    newtio.c_lflag &= ~ECHOE;           // Disable erase character echo
    newtio.c_lflag &= ~ECHOK;           // Disable kill character echo

    // c_oflag: Output modes
    newtio.c_oflag &= ~OPOST;           // Disable output post-processing (e.g., newline conversion)

    // c_iflag: Input modes
    newtio.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL); // Disable input processing

    // Set baud rate (use cfsetispeed/cfsetospeed for POSIX standard)
    cfsetispeed(&newtio, BAUDRATE);
    cfsetospeed(&newtio, BAUDRATE);

    // 4. Apply the new settings immediately
    tcsetattr(fd, TCSANOW, &newtio);
    //tcsetattr(fd, TCSANOW, &tty);
}


void WPortInit(){

    // 2. Get current terminal attributes
    if (tcgetattr(fdg, &tty) != 0) {
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
    if (tcsetattr(fdg, TCSANOW, &tty) != 0) {
        perror("Error from tcsetattr");
    }

}

void NtripSocketInit(char *host,char *httpreq){
     struct sockaddr_in server_addr;
     struct hostent *server;
    // 1. Create a socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) 
        perror("ERROR opening socket");

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
        perror("ERROR connecting");

    // 4. Send the HTTP request
    n = write(sockfd, get_request, strlen(httpreq));
    if (n < 0) 
        perror("ERROR writing to socket");
    
    printf("Sent HTTP Request:\n%s\n", httpreq);
    printf("--- Server Response ---\n");

    // 5. Read the server response (which includes HTTP response headers and body)
    bzero(buffer, BUFFER_SIZE);

}



