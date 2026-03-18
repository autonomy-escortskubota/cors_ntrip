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
void NTRIP_json_to_file(char const * fil,char *gga);
cJSON *parse_json_file(const char *filename);
char *read_file(const char *filename);
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
     cJSON *json_data = parse_json_file("ntrip_gga.json"); // Example filename
     if (json_data != NULL) {
        // Accessing a number item:
        cJSON *gga_item = cJSON_GetObjectItemCaseSensitive(json_data, "GGA");
        if (cJSON_IsString(gga_item) && (gga_item->valuestring != NULL)) {
        printf("NTRIP: %s\n", gga_item->valuestring);
        char *gga_in = gga_item->valuestring;
         write(sckfd,gga_in, strlen(gga_in));
         free(gga_in);
       }
    }
     
     
    
    bzero(buffer, BUFFER_SIZE);
    int k =1;
    while(k>0){
    pthread_mutex_lock(&lock);
    while (((n = read(sckfd, buffer, BUFFER_SIZE - 1)) > 0) && (server != NULL)){
     if (json_data != NULL) {
        // Accessing a number item:
       cJSON *json_data = parse_json_file("ntrip_gga.json"); // Example filename
        cJSON *gga_item = cJSON_GetObjectItemCaseSensitive(json_data, "GGA");
        if (cJSON_IsString(gga_item) && (gga_item->valuestring != NULL)) {
        printf("NTRIP: %s\n", gga_item->valuestring);
        char *gga_in = gga_item->valuestring;
         write(sckfd,gga_in, strlen(gga_in));
         free(gga_in);
       }
    }
  
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
	NTRIP_json_to_file("ntrip_gga.json",buf);
	}
	
	rmc_nmeaparser(buf,&tim,&valid,&lat,&ltdir,&lng,&lngdir,&spd,&hd,&dat,&fixt);
	//gga_nmeaparser(buf,&tim,&lat,&ltdir,&lng,&lngdir,&qua,&nsati,&hdop,&alti,&diff_age,&diff_sat);
	//printf("%d %c %f %c %f %c %f %f %d %c \r\n",tim,valid,lat,ltdir,lng,lngdir,spd,hd,dat,fixt);
	usleep(5000);
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


void NTRIP_json_to_file(char const * fil,char *gga)
{
    FILE *file = fopen(fil, "w");
    if (file == NULL) {
        fprintf(stderr, "Could not open %s for writing\n", fil);
        
    }


cJSON *monitor = cJSON_CreateObject();
cJSON_AddStringToObject(monitor,"GGA",gga);
char *json_string = cJSON_Print(monitor);
fprintf(file, "%s", json_string);
      // Close the file
    fclose(file);   
    cJSON_Delete(monitor);
    free(json_string);
    }
    
cJSON *parse_json_file(const char *filename) {
    char *content = read_file(filename);
    if (content == NULL) {
        return NULL;
    }

    cJSON *parsed_json = cJSON_Parse(content); // Parse the string into a cJSON object
    free(content); // Free the file content buffer

    if (parsed_json == NULL) {
        const char *error_ptr = cJSON_GetErrorPtr(); // Get the error position
        if (error_ptr != NULL) {
            fprintf(stderr, "Error before: %s\\n", error_ptr);
        }
        return NULL;
    }

    return parsed_json; // The caller is responsible for freeing the cJSON object
}


char *read_file(const char *filename) {
    FILE *fp = fopen(filename, "rb"); // Open file in binary read mode
    if (fp == NULL) {
        perror("Error opening file");
        return NULL;
    }

    fseek(fp, 0, SEEK_END); // Go to the end of the file
    long length = ftell(fp); // Get the file length
    fseek(fp, 0, SEEK_SET); // Go back to the start

    char *buffer = (char *)malloc(length + 1); // Allocate memory for the file content
    if (buffer == NULL) {
        perror("Error allocating memory");
        fclose(fp);
        return NULL;
    }

    size_t read_bytes = fread(buffer, 1, length, fp); // Read the file into the buffer
    if (read_bytes != (size_t)length) {
        perror("Error reading file");
        free(buffer);
        fclose(fp);
        return NULL;
    }

    buffer[length] = '\0'; // Null-terminate the string
    fclose(fp);

    return buffer; // The caller is responsible for freeing this memory
}



