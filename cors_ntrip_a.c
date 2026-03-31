#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
//#include <time.h>
#include <sys/socket.h>
#include <pthread.h>
#include "gnss.h"
#include <cjson/cJSON.h>
extern struct hostent *server;

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t buffer_mutex = PTHREAD_MUTEX_INITIALIZER;

void write_gga_to_file(const char *filename, char *gga);
char* read_gga_from_file(const char *filename);
void ntrip_gga(int sockfd, const char *filename);
// Define the host and the request
char *hst = "qrtksa1.quectel.com";   
// The HTTP request string must end with a double CRLF ("\r\n\r\n") to signify 
// the end of the headers.
char *get_request ="GET /AUTO_ITRF2020 HTTP/1.1\r\nHost: qrtksa1.quectel.com\r\nNtrip-Version: Ntrip/2.0\r\nUser-Agent: PythonNTRIP\r\nAuthorization: Basic ZXNjb3J0c2t1Ym90YV8wMDAwMDAxOm05amRlYjZ6\r\nConnection: keep-alive\r\n\r\n";

//char* gga;
void *ntrip_dev(void *arg){
    int wprt,sckfd,n;
    char buffer[BUFFER_SIZE];
    wprt = init_ntrip_write_port(DEVICE);
    sckfd = NtripSocketInit(hst,get_request);
     // 5. Read the server response (which includes HTTP response headers and body)
     ntrip_gga(sckfd,"ntrip.txt");
    
    bzero(buffer, BUFFER_SIZE);
    int k =1;
    while(k>0){
    pthread_mutex_lock(&lock);
    while (((n = read(sckfd, buffer, BUFFER_SIZE - 1)) > 0) && (server != NULL)){
    ntrip_gga(sckfd,"ntrip.txt");
     unsigned char* bytes = (unsigned char*)malloc(n);
    if (bytes == NULL) {
        perror("Failed to allocate memory");
    }
    // Copy the string data to the byte array
    memcpy(bytes,buffer,n);
    ssize_t z = write(wprt,bytes, n); // Exclude null terminator
     
	if (z < 0) {
    		perror("Error writing to serial port");
	} else {
    		printf("Wrote %zd bytes\n", z);
	}
	
    usleep(2000); // 500,000 microseconds = 0.5 seconds
    free(bytes); 
    // Free the allocated memory
    bzero(buffer, BUFFER_SIZE);
   
    }
    k+=1;
    if (n <= 0) {
        printf("Failed in SOCKET CONNECTION ---:%d\n",k);
        close(sckfd);
        sckfd = NtripSocketInit(hst,get_request);
         }
     pthread_mutex_unlock(&lock);
     
     }
      close(sckfd);
pthread_exit(NULL);
}

// The thread function that reads from the serial port
void *serial_reader_thread(void *arg) {
int hr,se,mi;
char gnss_t[64];
int res;
FILE *fdf = gnss_log();
int tim=0, dat=0,qua,nsati,diff_age,diff_sat;
double lat,lng,spd,hd;
float alti,hdop;
char valid,ltdir,lngdir,fixt;
char buf[BUFSIZE];
int fd = init_read_port(DEVICE);
pthread_mutex_lock(&buffer_mutex);
	while((res = read(fd, buf, sizeof(buf) - 1)) > 0){
	buf[res]='\0';
	if(verifyChecksum(buf)){
	if(strstr(buf,"$GNGGA")|| strstr(buf,"$GPGGA")){
	//NTRIP_json_to_file("ntrip_gga.json",buf);
	write_gga_to_file("ntrip.txt",buf);
	}
	rmc_nmeaparser(buf,&tim,&valid,&lat,&ltdir,&lng,&lngdir,&spd,&hd,&dat,&fixt);
	gga_nmeaparser(buf,&tim,&lat,&ltdir,&lng,&lngdir,&qua,&nsati,&hdop,&alti,&diff_age,&diff_sat);
	if(lat != 0.0 && lng != 0.0 && alti !=0.0 ){
        convert_time_to_UTC((unsigned)tim,&hr,&mi,&se);
        sprintf(gnss_t,"%02d:%02d:%02d",hr,mi,se);
	write_json_to_file("dev.json",gnss_t,lat,lng,alti,hdop,spd,hd,qua,nsati,diff_age,dat,diff_sat);
	usleep(5000);
	}
   }
}
pthread_mutex_unlock(&buffer_mutex);
close_port(fd);
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
    if (pthread_create(&tid, NULL,ntrip_dev,NULL) != 0) {
        fprintf(stderr, "Error creating thread\\n");
       return 1;
    }
    
    
   if (pthread_create(&wid, NULL, serial_reader_thread, NULL) != 0) {
        fprintf(stderr, "READING Error creating thread\\n");
        return 1;
    }
    
    

    // Clean up (in a real app you'd loop and manage termination)
    pthread_join(tid, NULL); // Wait for the reader thread to finish
    // Clean up (in a real app you'd loop and manage termination)
    pthread_join(wid, NULL); // Wait for the reader thread to finish
    pthread_mutex_destroy(&buffer_mutex);
    pthread_mutex_destroy(&lock);
    
    return 0;

}

// =====================================================
//            FILE WRITE
// =====================================================
void write_gga_to_file(const char *filename, char *gga)
{


    FILE *file = fopen(filename, "w");
    if (file == NULL) {
        perror("Error opening GGA file");

        return;
    }

    fprintf(file, "%s", gga);
    fclose(file);

}

// =====================================================
//            FILE READ
// =====================================================
char* read_gga_from_file(const char *filename)
{


    FILE *file = fopen(filename, "r");
    if (file == NULL) {

        return NULL;
    }

    static char buffer[256];

    if (fgets(buffer, sizeof(buffer), file) == NULL) {
        fclose(file);

        return NULL;
    }

    fclose(file);
    return buffer;
}

// =====================================================
//            SEND GGA TO NTRIP
// =====================================================
void ntrip_gga(int sockfd, const char *filename)
{
    char *gga = read_gga_from_file(filename);

    if (gga != NULL) {
        printf("NTRIP GGA: %s\n", gga);
        write(sockfd, gga, strlen(gga));
    }
}
