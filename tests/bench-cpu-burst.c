#define _GNU_SOURCE
#include <time.h>
#include <stdint.h>
#include <stdio.h>
#include <sched.h>
#include <stdlib.h>
static double now(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec*1e-9;}
int main(int argc,char **argv){cpu_set_t set;CPU_ZERO(&set);CPU_SET(argc>1?atoi(argv[1]):4,&set);if(sched_setaffinity(0,sizeof(set),&set))return 1;volatile uint64_t result=1;struct timespec pause={0,100000000};for(int n=0;n<40;n++){nanosleep(&pause,0);uint64_t x=result;double start=now();for(int i=0;i<2000000;i++){x^=x<<13;x^=x>>7;x^=x<<17;}double end=now();result=x;printf("%.3f\n",(end-start)*1000);}return result==0;}
