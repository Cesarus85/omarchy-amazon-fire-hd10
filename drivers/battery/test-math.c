#include <assert.h>
#include <stdio.h>
#include "trona_battery_math.h"
int main(void) {
 int variant,temp,ua,v,full,q,previous;
 assert(trona_current_ua(0)==0 && trona_current_ua(65535)==0);
 assert(trona_current_ua(1000)==381470);
 assert(trona_current_ua(64535)==-381470);
 assert(trona_current_ua(0xff1c)==-86593);
 assert(trona_fg_ticks(0xffffffff)==0);
 assert(trona_fg_ticks(0xfffffffe)==-1);
 assert(trona_tick_delta(-2147483647LL,2147483647LL)==1);
 assert(trona_tick_delta(2147483647LL,-2147483647LL)==-1);
 assert(trona_temp_deci_c(972)>=280 && trona_temp_deci_c(972)<=300);
 assert(trona_temp_deci_c(0)==-10000);
 for(variant=0;variant<3;variant++) for(temp=-200;temp<=600;temp+=25)
  for(ua=-2000000;ua<=2000000;ua+=250000) {
   previous=-1;
   for(v=2800000;v<=4700000;v+=5000) {
    q=trona_estimate_charge(variant,temp,v,ua,&full);
    assert(q>=0 && q<=full && q>=previous);
    previous=q;
   }
  }
 q=trona_estimate_charge(1,290,3785000,-90000,&full);
 assert(full==5500000 && q>2000000 && q<3500000);
 printf("Fuel-gauge units, signed wrap, thermistor and all 15 curves passed; sample %d/%d uAh\n",q,full);
 return 0;
}
