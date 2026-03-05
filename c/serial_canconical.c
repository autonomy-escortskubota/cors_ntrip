#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>

#define BAUDRATE B115200
#define BUFFER_SIZE 255
struct termios oldtio, newtio;
int fd;
void PortInit();
void close_port();
int main() {
    char buf[BUFFER_SIZE];
    int bytes_read;
    
    fd = open("/dev/ttyUSB0", O_RDWR | O_NOCTTY );
    if (fd < 0) {
        perror("Failed to open serial device");
        exit(EXIT_FAILURE);
    }
    PortInit();
    // 5. Read data line by line
    printf("Reading from serial port (waiting for newline character)...\\n");
    while ((bytes_read = read(fd, buf, sizeof(buf) - 1)) > 0) {
        // null-terminate the string for standard C string functions
        buf[bytes_read] = '\0'; 
        printf("Received line (%d bytes): %s", bytes_read, buf);
        
    }

    if (bytes_read < 0) {
        perror("Error reading from serial port");
    }
    //close_port(port);

    return 0;
}
void PortInit(){

    // 2. Get current serial port settings and store for later restoration
    tcgetattr(fd, &oldtio);
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
}


void close_port(){

    // 6. Restore original settings and close the port
    tcsetattr(fd, TCSANOW, &oldtio);
    close(fd);
    printf("Port closed and settings restored.\\n");

}
