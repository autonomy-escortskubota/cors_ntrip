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
#include <stdbool.h>
#include <cjson/cJSON.h>
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
char *hst = "qrtksa1.quectel.com";   
// The HTTP request string must end with a double CRLF ("\r\n\r\n") to signify 
// the end of the headers.
char *get_request ="GET /AUTO_ITRF2020 HTTP/1.1\r\nHost: qrtksa1.quectel.com\r\nNtrip-Version: Ntrip/2.0\r\nUser-Agent: PythonNTRIP\r\nAuthorization: Basic ZXNjb3J0c2t1Ym90YV8wMDAwMDAxOm05amRlYjZ6\r\nConnection: keep-alive\r\n\r\n";

void WPortInit();
void NtripSocketInit(char *host,char *httpreq);
void configure_serial_port(int fd);
double lng_filt(float kef);
double lat_filt(float def);
void gnss_filter(char *str_g,int *tim_g,char *valid, double *lat_g,char *latdir, double *lng_g,char *lngdir,double *alt, double *hdop, double *spd, double *head,int *gga_qa,int *gga_numsv, int *gga_diff_age,int *dat);
void convert_time_to_UTC(unsigned int UTC_Time,int *hrk ,int *mink,int *seck);
void write_json_to_file(const char *filename,int val,char *timj,double latj, double lngj,double altj, double hdopj, double spdj, double headj,int gga_qaj,int gga_numsvj, int gga_diff_agej,int datj );

struct sockaddr_in server_addr;
struct hostent *server;
struct timeval timeout; 
char *gga;
void *ntrip_dev(void *arg){
    const char *portname = "/dev/ttyUSB0"; // Change to your serial port (e.g., /dev/ttyS0, /dev/ttyAMA0)

 fdg = open(portname, O_RDWR | O_NOCTTY | O_SYNC);
    if (fdg < 0) {
        perror("Error opening serial port----- Write");
    }
    WPortInit(); 
    NtripSocketInit(hst,get_request);
    int k =1;
    while(k>0){
    pthread_mutex_lock(&lock);
    //printf("Match found! The first word is indeed '%s'.\n",gga);
    
    while (((n = read(sockfd, buffer, BUFFER_SIZE - 1)) > 0) && (server != NULL)){
      // 4. Send the HTTP request
             //printf("%s \n",buffer);
            if (strncmp(gga,"$GNGGA",6) == 0) {
                 
                //read(serial_fd, serial_buffer, sizeof(serial_buffer) - 1);
                write(sockfd, gga, strlen(gga));
           	printf("Match found! The first word is indeed '%s'.\n",gga);
    		}
  
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
	
    //usleep(500); // 500,000 microseconds = 0.5 seconds
    free(bytes); 
    // Free the allocated memory
    bzero(buffer, BUFFER_SIZE);
   
    }
    k+=1;
    if (n < 0) {
        //perror("ERROR reading from socket");
        printf("Failed in SOCKET CONNECTION ---:%d\n",k);
        close(sockfd);
        sleep(10);
        NtripSocketInit(hst,get_request);
       //continue;
         }
     pthread_mutex_unlock(&lock);
     
     }
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
    int gnss_t,g_numst,g_diffsat,date_g;
    double gnss_lat,gnss_lng,rmc_spd,rmc_head,gga_alt,gga_hdop;
    int g_qa;
    char l_dir,lng_dir,v_gnss;
    int hr,se,mi;
    char gns_time[64];
    configure_serial_port(serial_fd);
   
    while (1) {
        // Read data from the serial port
        pthread_mutex_lock(&buffer_mutex);
        
        int count = read(serial_fd, serial_buffer, sizeof(serial_buffer) - 1);
        
        gga = serial_buffer;
        if (count > 0) {
            serial_buffer[count] = '\0'; // Null-terminate for string handling
          // printf("Data received: %s\\n", serial_buffer);
            ///line = strtok(filt, "\n");
            //printf(" %s\n",line);
            ///char *first_word = strtok(line,",");
          //read(serial_fd, serial_buffer, sizeof(serial_buffer) - 1);
         
    	//printf("Match found! The first word is indeed '%s'.\n",filt);
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
    
        if (pthread_mutex_init(&buffer_mutex, NULL) != 0) {
        fprintf(stderr, "Mutex init failed\n");
        return 1;
    }

    
     // Create the reader thread
    if (pthread_create(&tid, NULL,ntrip_dev, NULL) != 0) {
        fprintf(stderr, "Error creating thread\\n");
        close(sockfd);
    }
    
    
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

       int optval = 1;
         
    timeout.tv_sec = 60;
    timeout.tv_usec = 0;

        // 1. Create a socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
         // Set the receive timeout option on the socket
    
    if (setsockopt (sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout,
                sizeof timeout) < 0){
        perror("setsockopt failed\n");}

    if (setsockopt (sockfd, SOL_SOCKET, SO_SNDTIMEO, &timeout,
                sizeof timeout) < 0){
        perror("setsockopt failed\n");}
        
    if (setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval)) < 0) {
    perror("setsockopt(SO_KEEPALIVE)");
    } 

    if (sockfd < 0) 
        perror("ERROR opening socket");

    // 2. Get server details
    server = gethostbyname(host);
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host\n");
        //exit(1);
        close(sockfd);
        sleep(10); 
        NtripSocketInit(hst,get_request);
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
   
                //read(serial_fd, serial_buffer, sizeof(serial_buffer) - 1);
    write(sockfd,"$GNGGA,042333.000,2825.9700300,N,07718.4236800,E,1,25,0.88,175.32,M,-36.34,M,,*5F", strlen("$GNGGA,042333.000,2825.9700300,N,07718.4236800,E,1,25,0.88,175.32,M,-36.34,M,,*5F"));

    
    printf("Sent HTTP Request:\n%s\n", httpreq);
    printf("--- Server Response ---\n");

    // 5. Read the server response (which includes HTTP response headers and body)
    bzero(buffer, BUFFER_SIZE);

}

void gnss_filter(char *str_g,int *tim_g,char *valid, double *lat_g,char *latdir, double *lng_g,char *lngdir,double *alt, double *hdop, double *spd, double *head,int *gga_qa,int *gga_numsv, int *gga_diff_age,int *dat){

/*-------------------------------------------------------*/
        
        char *tok = strtok(str_g, "$");
        while (tok != NULL) {
        
        int id_ga = 0; // Initialize the flag as true
        bool id = false; // Initialize the flag as true
        int j=0;
        char *netg = strtok(tok, ",");
        while (netg != NULL) {
              j+=1;

              if (j==1){
              if(strcmp(netg,"GNGGA")==0){
               id_ga = 1;
                    }
             if(strcmp(netg,"GNRMC")==0){
               id = true;
                   }    
                      
              }
              if(j==2 && id){
              *tim_g = atoi(netg);
              }
              
              if(j==3 && id){
              *valid = netg[0];
              }
               if(j==4 && id){
              *lat_g = lat_filt(atof(netg));
              }
              if(j==5 && id){
               *latdir = netg[0];
              }
              if(j==6 && id){
              *lng_g = lng_filt(atof(netg));
              }
              if(j==7 && id){
              *lngdir=netg[0];
              }
              if(j==8 && id){
              *spd=atof(netg);
              }
              if(j==9 && id){
              *head=atof(netg);
              }
              if(j==10 && id){
              *dat=atoi(netg);
              }

             /* if(i==13 && id){
               *rmc_fix=netg[0];
              } */  
              
              if((j==2) && (id_ga==1)){
               *tim_g=atoi(netg);
              }
              
              if((j==3) && (id_ga==1)){
             *lat_g=lat_filt(atof(netg));
              }
              
              if((j==4) && (id_ga==1)){
              *latdir=netg[0];
              }
              
              if((j==5) && (id_ga==1)){
               *lng_g=lng_filt(atof(netg));             
              }
             
              if((j==6) && (id_ga==1)){
              *lngdir=netg[0];
              }
              
              if((j==7) && (id_ga==1)){
             *gga_qa=atoi(netg);
              }
              
              if((j==8) && (id_ga==1)){
              *gga_numsv=atoi(netg);
              }
              
              if((j==9) && (id_ga==1)){
              *hdop=atof(netg);
              }
              
              if((j==10) && (id_ga==1)){
               *alt=atof(netg);
              }
              
              if((j==14) && (id_ga==1)){
              *gga_diff_age=atoi(netg);
              }   
   
             netg = strtok(NULL,",");
             
         }
        
        // Subsequent calls to strtok() must pass NULL as the first argument
        tok = strtok(NULL, "$"); 
       }
}











double lng_filt(float kef){
    float longitude = 0.0;
    float k_lng_deg=(kef*0.01);
    unsigned int deglng = (int)k_lng_deg;
    if(deglng > 68 && 97 > deglng){
        float seclng = (kef- (float)deglng*100)/60;
        longitude = (float)deglng + seclng;

    }

    return longitude;
}

double lat_filt(float def) {
    float latitude = 0.0;
    float k_lat_deg=(def*0.01);
    unsigned int deg = (int)k_lat_deg;
    if(deg > 8 && 37 > deg){
        float sec = (def- (float)deg*100)/60;
        latitude = (float)deg + sec;
    }

    return latitude;
}



void convert_time_to_UTC(unsigned int UTC_Time,int *hrk ,int *mink,int *seck) {
	
	unsigned int hour, min, sec;

	hour = (UTC_Time / 10000);
	min = (UTC_Time % 10000) / 100;
	sec = (UTC_Time % 10000) % 100;
	hour = hour+5;
	if (hour > 23)
	{
		hour = 23-hour;
		
	}
	min = min + 30;
	if (min > 59)
	{
		min = min-60;
		hour+=1;
	}
        *hrk = (int)hour;*mink = (int)min;*seck = (int)sec;
}

void write_json_to_file(const char *filename,int val,char *timj,double latj, double lngj,double altj, double hdopj, double spdj, double headj,int gga_qaj,int gga_numsvj, int gga_diff_agej,int datj ) {
    cJSON *monitor = cJSON_CreateObject();
    cJSON_AddStringToObject(monitor,"Time", timj);
    cJSON_AddNumberToObject(monitor,"Latitude", latj);
    cJSON_AddNumberToObject(monitor,"Longitude", lngj);
    cJSON_AddNumberToObject(monitor,"Altitude", altj);
    cJSON_AddNumberToObject(monitor,"Head", headj);
    cJSON_AddNumberToObject(monitor,"Speed", spdj);
    cJSON_AddNumberToObject(monitor,"HDOP", hdopj);
    cJSON_AddNumberToObject(monitor,"Quality", gga_qaj);
    cJSON_AddNumberToObject(monitor,"Number of Satellite", gga_numsvj);
    cJSON_AddNumberToObject(monitor,"Differential Age", gga_diff_agej);
    cJSON_AddNumberToObject(monitor,"Date", datj);
    cJSON_AddNumberToObject(monitor,"NTRIP", val);
    char *json_string = cJSON_Print(monitor);
    FILE *file = fopen(filename, "w");
    if (file != NULL) {
        fputs(json_string, file);
        fclose(file);
        //printf("Successfully wrote JSON to output.json\n");
    } else {
        perror("Error opening file");
    }


    
    printf("%s \n",json_string);
    //fprintf(file, "%s", json_string);
    

    //fclose(file);
    cJSON_Delete(monitor);
    free(json_string);
}
