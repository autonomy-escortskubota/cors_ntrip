#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#include <stdbool.h> // Include standard boolean definitions
#include <cjson/cJSON.h>

#define BAUDRATE B115200
#define BUFFER_SIZE 255
struct termios oldtio, newtio;
struct gnss_rmc{
char rmc_id;
unsigned int rmc_time ;
unsigned int rmc_date ;
char rmc_valid;
double latitude;
char lat_c;
double lngitude;
char lng_c;
double rmc_speed;
double rmc_heads;
char rmc_fix;
};

struct gnss_gga{
char gga_id;
unsigned int gga_time ;
unsigned int gga_qa ;
char gga_altdir;
double gga_lat;
char gga_latdir;
double gga_lng;
char gga_lngdir;
double gga_hdop;
double gga_alt;
int gga_diff_age;
int gga_numsv;
};


int fd;
struct gnss_rmc rmc_s(char *inp);
struct gnss_gga gga_s(char *i);
void write_json_to_file(const char *filename,double latj, double lngj, char *tim,double headj,double spdj);
void PortInit();
void close_port();
double lng_filt(float kef);
double lat_filt(float def);
void gnss_filter(char *str_g,int *tim_g,char *valid, double *lat_g,char *latdir, double *lng_g,char *lngdir,double *alt, double *hdop, double *spd, double *head,int *gga_qa,int *gga_numsv, int *gga_diff_age,int *dat);
void convert_time_to_UTC(unsigned int UTC_Time,int *hrk ,int *mink,int *seck);
void rmc_filter(char *str,char *id,int *tim,double *lat, double *lng,double *spd, double *head);
void gga_filter(char *str_g,char *id_g,int *tim_g,double *lat_g, double *lng_g,double *alt, double *hdop);
int main() {
    char buf[BUFFER_SIZE];
    int bytes_read;
    char *idvg = "GNGGA";
    char *idvr = "GNRMC";
    int gnss_t;
    double gnss_lat;
    double gnss_lng;
    double rmc_spd;
    double rmc_head;
    double gga_alt;
    double gga_hdop;
    int g_qa;
    int g_numst;
    int g_diffsat;
    char l_dir;
    char lng_dir;
    int date_g;
    char v_gnss;
    
    int hr,se,mi;
    char gns_time[64];
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
   /*       char* bytes = (char*)malloc(bytes_read);
    if (bytes == NULL) {
        perror("Failed to allocate memory");

    }
            // Copy the string data to the byte array
        memcpy(bytes,buf,bytes_read);
        
        struct gnss_rmc rmc_o = rmc_s(bytes);
        if(rmc_o.latitude !=0.0 && rmc_o.lngitude !=0.0 && rmc_o.rmc_time!=0 &&  rmc_o.rmc_speed != 0.0){
        printf("%f,%f,%d,%f %f \n",rmc_o.latitude, rmc_o.lngitude,rmc_o.rmc_time,rmc_o.rmc_heads,rmc_o.rmc_speed);
        convert_time_to_UTC((unsigned)rmc_o.rmc_time,&hr,&mi,&se);
        sprintf(gns_time,"%02d:%02d:%02d",hr,mi,se);
        write_json_to_file("test.json",rmc_o.latitude, rmc_o.lngitude,gns_time,rmc_o.rmc_heads,rmc_o.rmc_speed);
        }
        struct gnss_gga gga_o = gga_s(bytes);
        if(gga_o.gga_lat !=0.0 && gga_o.gga_lng !=0.0 && gga_o.gga_time!=0 &&  gga_o.gga_alt != 0.0){
        printf("GGA:%f,%f,%d,%f %f \n",gga_o.gga_lat, gga_o.gga_lng,gga_o.gga_time,gga_o.gga_alt,gga_o.gga_hdop);
        }*/
        
       /* gnss_filter(buf,idv,&rmc_t,&rmc_lat,&rmc_lng,&rmc_spd,&rmc_head);
        convert_time_to_UTC((unsigned)rmc_t,&hr,&mi,&se);
        sprintf(gns_time,"%02d:%02d:%02d",hr,mi,se);
        write_json_to_file("test.json",rmc_lat, rmc_lng,gns_time,rmc_head,rmc_spd);*/
        //rmc_filter(buf,idvr,&gnss_t,&gnss_lat,&gnss_lng,&rmc_spd,&rmc_head);
        //gga_filter(buf,idvg,&gnss_t,&gnss_lat,&gnss_lng,&gga_alt,&gga_hdop);
        gnss_filter(buf,&gnss_t,&v_gnss,&gnss_lat,&l_dir,&gnss_lng,&lng_dir,&gga_alt,&gga_hdop,&rmc_spd,&rmc_head,&g_qa,&g_numst,&g_diffsat,&date_g);
        printf("%f,%f,%d,%f %f %f %f %d %d %d %d \n",gnss_lat,gnss_lng,gnss_t,rmc_spd,rmc_head,gga_alt,gga_hdop,g_qa,g_numst,g_diffsat,date_g);
     // printf("out:%d %d \n",rmc_o.rmc_time,rmc_o.rmc_date);

    /*-----------------------------------------------------------*/    
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

void rmc_filter(char *str,char *idg, int *tim,double *lat, double *lng,double *spd, double *head){

        /*-------------------------------------------------------*/

        char *token = strtok(str, "$");
        printf("%s \n",str);
        while (token != NULL) {
        
        bool id = false; // Initialize the flag as true
        int i=0;
        char *net = strtok(token, ",");
            // Walk through other tokens using a loop
        while (net != NULL) {
              i+=1;
              
              if (i==1){
              
              if(strcmp(net, idg)==0){
               id = true;
               
                   }
              }
              else if(i==2 && id){
              *tim = atoi(net);
              }
                          /*  else if(i==3 && id){
              printf("%s\n", net);
              }*/
                            else if(i==4 && id){
              *lat = lat_filt(atof(net));
              }
                           /* else if(i==5 && id){
              printf("%s\n", net);
              }*/
                            else if(i==6 && id){
              *lng = lng_filt(atof(net));
              }
                            /*else if(i==7 && id){
              printf("%s\n", net);
              }*/
                            else if(i==8 && id){
              *spd=atof(net);
              }
                            else if(i==9 && id){
              *head=atof(net);
              }
              	           /* else if(i==10 && id){
              printf("%s\n", net);
              }
                             else if(i==11 && id){
              printf("%s\n", net);
              }
                             else if(i==12 && id){
              printf("%s\n", net);
              }   */ 
   
             net = strtok(NULL,",");
             
         }
        
        // Subsequent calls to strtok() must pass NULL as the first argument
        token = strtok(NULL, "$"); 
    }

}
struct gnss_rmc rmc_s(char *inp){
struct gnss_rmc rmc_i;
       /*-------------------------------------------------------*/
        
        char *token = strtok(inp, "$");
        while (token != NULL) {
        
        int id_r = 0; // Initialize the flag as true
        int j=0;
        char *net = strtok(token, ",");
        while (net != NULL) {
              j+=1;

              if (j==1){
              if(strcmp(net,"GNRMC")==0){
               id_r = 1;
                    }
                 
              }
              
              if((j==2) && (id_r==1)){
              printf("%d \n",id_r);
              rmc_i.rmc_time=atoi(net);
              }
              
              if((j==3) && (id_r==1)){
              rmc_i.rmc_valid=net[0];
              }
              
             if((j==4) && (id_r==1)){
              rmc_i.latitude=lat_filt(atof(net));
              }
              
              if((j==5) && (id_r==1)){
               rmc_i.lat_c=net[0];             
              }
              
              if((j==6) && (id_r==1)){
              rmc_i.lngitude = lng_filt(atof(net));
              }
              
              if((j==7) && (id_r==1)){
             rmc_i.lng_c=net[0];
              }
              
              if((j==8) && (id_r==1)){
              rmc_i.rmc_speed=atof(net);
              }
              
              if((j==9) && (id_r==1)){
              rmc_i.rmc_heads=atof(net);
              }
              
              if((j==10) && (id_r==1)){
               rmc_i.rmc_date=atoi(net);
              }
              
              if((j==12) && (id_r==1)){
              rmc_i.rmc_fix=net[0];
              }    
   
             net = strtok(NULL,",");
             
         }
        
        // Subsequent calls to strtok() must pass NULL as the first argument
        token = strtok(NULL, "$"); 
       }


    return rmc_i;
}


struct gnss_gga gga_s(char *i){
struct gnss_gga gga_i;
       /*-------------------------------------------------------*/
        
        char *tok = strtok(i, "$");
        while (tok != NULL) {
        
        int id_g = 0; // Initialize the flag as true
        int j=0;
        char *netg = strtok(tok, ",");
        while (netg != NULL) {
              j+=1;

              if (j==1){
              if(strcmp(netg,"GNGGA")==0){
               id_g = 1;
                    }
                 
              }
              
              if((j==2) && (id_g==1)){
              gga_i.gga_time=atoi(netg);
              }
              
              if((j==3) && (id_g==1)){
              gga_i.gga_lat=lat_filt(atof(netg));
              }
              
             if((j==4) && (id_g==1)){
              gga_i.gga_latdir=netg[0];
              }
              
              if((j==5) && (id_g==1)){
               gga_i.gga_lng=lng_filt(atof(netg));             
              }
              
              if((j==6) && (id_g==1)){
              gga_i.gga_lngdir=netg[0];
              }
              
              if((j==7) && (id_g==1)){
             gga_i.gga_qa=atoi(netg);
              }
              
              if((j==8) && (id_g==1)){
              gga_i.gga_numsv=atoi(netg);
              }
              
              if((j==9) && (id_g==1)){
              gga_i.gga_hdop=atof(netg);
              }
              
              if((j==10) && (id_g==1)){
               gga_i.gga_alt=atof(netg);
              }
              
              if((j==14) && (id_g==1)){
              gga_i.gga_diff_age=atoi(netg);
              }    
   
             netg = strtok(NULL,",");
             
         }
        
        // Subsequent calls to strtok() must pass NULL as the first argument
        tok = strtok(NULL, "$"); 
       }


    return gga_i;
}

void gga_filter(char *str_g,char *id_g,int *tim_g,double *lat_g, double *lng_g,double *alt, double *hdop){

/*-------------------------------------------------------*/
        
        char *tok = strtok(str_g, "$");
        while (tok != NULL) {
        
        int id_ga = 0; // Initialize the flag as true
        int j=0;
        char *netg = strtok(tok, ",");
        while (netg != NULL) {
              j+=1;

              if (j==1){
              if(strcmp(netg,id_g)==0){
               id_ga = 1;
                    }
                 
              }
              
              if((j==2) && (id_ga==1)){
               *tim_g=atoi(netg);
              }
              
              if((j==3) && (id_ga==1)){
             *lat_g=lat_filt(atof(netg));
              }
              
          /*   if((j==4) && (id_ga==1)){
              gga_i.gga_latdir=netg[0];
              }*/
              
              if((j==5) && (id_ga==1)){
               *lng_g=lng_filt(atof(netg));             
              }
              
             /* if((j==6) && (id_ga==1)){
              gga_i.gga_lngdir=netg[0];
              }
              
              if((j==7) && (id_ga==1)){
             gga_i.gga_qa=atoi(netg);
              }
              
              if((j==8) && (id_ga==1)){
              gga_i.gga_numsv=atoi(netg);
              }*/
              
              if((j==9) && (id_ga==1)){
              *hdop=atof(netg);
              }
              
              if((j==10) && (id_ga==1)){
               *alt=atof(netg);
              }
           /*   
              if((j==14) && (id_ga==1)){
              gga_i.gga_diff_age=atoi(netg);
              }   */ 
   
             netg = strtok(NULL,",");
             
         }
        
        // Subsequent calls to strtok() must pass NULL as the first argument
        tok = strtok(NULL, "$"); 
       }
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
	if (hour >= 23)
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

void write_json_to_file(const char *filename,double latj, double lngj, char *tim,double headj,double spdj) {
    cJSON *monitor = cJSON_CreateObject();
    cJSON_AddStringToObject(monitor,"Time", tim);
    cJSON_AddNumberToObject(monitor,"Latitude", latj);
    cJSON_AddNumberToObject(monitor,"Longitude", lngj);
    cJSON_AddNumberToObject(monitor,"Head", headj);
    cJSON_AddNumberToObject(monitor,"Speed", spdj);

    FILE *file = fopen(filename, "w");
    if (file == NULL) {
        fprintf(stderr, "Could not open %s for writing\n", filename);
        cJSON_Delete(monitor);
        return;
    }

    char *json_string = cJSON_Print(monitor);
    printf("%s \n",json_string);
    fprintf(file, "%s", json_string);
    

    fclose(file);
    cJSON_Delete(monitor);
    free(json_string);
}




