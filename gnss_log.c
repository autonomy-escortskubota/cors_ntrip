#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
//#include <time.h>
#include "gnss.h"
#include <cjson/cJSON.h>


int main(){
int hr,se,mi;
char gnss_t[64];
FILE *fdf = gnss_log(); 
int res;
int tim=0, dat=0,qua,nsati,diff_age,diff_sat;
double lat,lng,spd,hd;
float alti,hdop;
char valid,ltdir,lngdir,fixt;
char buf[BUFSIZE];
int fd = init_read_port(DEVICE);

while((res = read(fd, buf, sizeof(buf) - 1)) > 0){
buf[res]='\0';

if(verifyChecksum(buf)){
rmc_nmeaparser(buf,&tim,&valid,&lat,&ltdir,&lng,&lngdir,&spd,&hd,&dat,&fixt);
gga_nmeaparser(buf,&tim,&lat,&ltdir,&lng,&lngdir,&qua,&nsati,&hdop,&alti,&diff_age,&diff_sat);
if(lat != 0.0 && lng != 0.0 && alti !=0.0 ) {
convert_time_to_UTC((unsigned)tim,&hr,&mi,&se);
sprintf(gnss_t,"%02d:%02d:%02d",hr,mi,se);
log_json_to_file(fdf,gnss_t,lat,lng,alti,hdop,spd,hd,qua,nsati,diff_age,dat,diff_sat);
usleep(5000);
   }

   }
}
close_port(fd);
}


