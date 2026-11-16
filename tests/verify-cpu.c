// SPDX-License-Identifier: GPL-2.0-only
/* Eight pinned workers: deterministic integer, memory and FP verification.
 * Run only during an attended thermal qualification; this saturates the CPU. */
#define _GNU_SOURCE
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sched.h>
#include <time.h>
#include <sys/wait.h>
static double now(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec*1e-9;}
int main(int argc,char **argv){
 int seconds=argc>1?atoi(argv[1]):30, failures=0;
 for(int cpu=0;cpu<8;cpu++)if(fork()==0){
  cpu_set_t set;CPU_ZERO(&set);CPU_SET(cpu,&set);if(sched_setaffinity(0,sizeof(set),&set))_exit(2);
  uint64_t *buf=malloc(4*1024*1024);if(!buf)_exit(3);
  uint64_t rounds=0;double end=now()+seconds;
  while(now()<end){
   uint64_t x=1;
   for(unsigned i=0;i<100000;i++){x^=x<<13;x^=x>>7;x^=x<<17;buf[i]=x;}
   if(x!=0x25797e2f2c31a5cbULL){fprintf(stderr,"CPU%d arithmetic mismatch\n",cpu);_exit(4);}
   x=1;for(unsigned i=0;i<100000;i++){x^=x<<13;x^=x>>7;x^=x<<17;if(buf[i]!=x)_exit(5);}
   volatile double dot=0;for(unsigned i=0;i<10000;i++)dot+=(double)(i%97)*(double)(i%89);
   if(dot!=21129459.0)_exit(6);
   rounds++;
  }
  printf("cpu=%d verified_rounds=%llu seconds=%d\n",cpu,(unsigned long long)rounds,seconds);fflush(stdout);_exit(0);
 }
 for(int cpu=0;cpu<8;cpu++){int st;wait(&st);if(!WIFEXITED(st)||WEXITSTATUS(st))failures++;}
 printf("failures=%d\n",failures);return !!failures;
}
