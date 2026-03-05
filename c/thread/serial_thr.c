#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#include <pthread.h>
#define BAUDRATE B115200
#define BUFFER_SIZE 256

// Shared resources
char serial_buffer[BUFFER_SIZE];
size_t bytes_read = 0;
pthread_mutex_t buffer_mutex = PTHREAD_MUTEX_INITIALIZER;
int serial_fd = -1; // File descriptor for the serial port

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

// The thread function that reads from the serial port
void *serial_reader_thread(void *arg) {
    // int fd = *((int*)arg); // If passing fd directly as a pointer
    // Use the global serial_fd instead for simplicity
    
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

int main() {
    pthread_t tid;
    const char* serial_port_path = "/dev/ttyUSB0"; // Replace with your port (e.g., /dev/ttyUSB0, /dev/ttyACM0)

    // Open the serial port
    serial_fd = open(serial_port_path, O_RDWR | O_NOCTTY );
    if (serial_fd < 0) {
        perror("Error opening serial port");
        return 1;
    }

    configure_serial_port(serial_fd);

    // Create the reader thread
    if (pthread_create(&tid, NULL, serial_reader_thread, NULL) != 0) {
        fprintf(stderr, "Error creating thread\\n");
        close(serial_fd);
        return 1;
    }

    // Clean up (in a real app you'd loop and manage termination)
    pthread_join(tid, NULL); // Wait for the reader thread to finish
    close(serial_fd);
    pthread_mutex_destroy(&buffer_mutex);
 

    return 0;
}


