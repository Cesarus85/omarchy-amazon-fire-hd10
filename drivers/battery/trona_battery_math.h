/* SPDX-License-Identifier: GPL-2.0-only */
#include "trona_battery_profile.h"
static long long trona_fg_ticks(unsigned int raw)
{
 return raw & 0x80000000U ? (long long)(raw & 0x7fffffffU) - 0x7fffffffLL : raw;
}
static int trona_current_ua(unsigned int raw)
{
 int signed_raw = raw > 32767 ? (int)raw - 65535 : (int)raw;
 /* Stock 10 milliohm shunt and unity CAR_TUNE_VALUE. */
 return (long long)signed_raw * 381470 / 1000;
}
static long long trona_tick_delta(long long now, long long old)
{
 long long d = now - old;
 if (d > 0x7fffffffLL) d -= 0xffffffffLL;
 if (d < -0x7fffffffLL) d += 0xffffffffLL;
 return d;
}
static int trona_temp_deci_c(int raw)
{
 static const int resistance[] = {1747920,1245428,898485,655802,483954,
  360850,271697,206463,158214,122259,95227,74730,59065,47000,37643,
  30334,24591,20048,16433,13539,11210};
 int mv = raw * 3600 / 4096, ohm, i;
 if (mv <= 0 || mv >= 2790) return -10000;
 ohm = (long long)90000 * mv / (2800 - mv);
 if (ohm >= resistance[0]) return -400;
 for (i=1;i<21;i++)
  if (ohm >= resistance[i])
   return -400 + (i-1)*50 + (long long)(resistance[i-1]-ohm)*50 /
          (resistance[i-1]-resistance[i]);
 return 600;
}
static int trona_curve_voltage(const struct trona_curve_point *p, int current_ua)
{
 /* Stock resistance: 0.1 milliohm; shunt 100 + meter 75 in same units. */
 return p->voltage * 100 + (long long)current_ua * (p->resistance+175) / 10000;
}
static int trona_curve_charge(int variant, int t, int voltage_uv, int current_ua)
{
 const struct trona_curve_point *p = profiles[variant][t];
 int i, prev = trona_curve_voltage(&p[0],current_ua), next;
 long long discharged = 0, charge;
 if (voltage_uv < prev) {
  discharged = p[99].discharged * 100LL;
  for (i=1;i<100;i++) {
   next = trona_curve_voltage(&p[i],current_ua);
   if (next > prev) next = prev;
   if (voltage_uv >= next && prev > next) {
    discharged = p[i-1].discharged * 100LL +
      (long long)(prev-voltage_uv)*(p[i].discharged-p[i-1].discharged)*100/(prev-next);
    break;
   }
   prev = next;
  }
 }
 charge = profile_full_uah[variant][t] - discharged;
 if (charge < 0) return 0;
 if (charge > profile_full_uah[variant][t]) return profile_full_uah[variant][t];
 return charge;
}
static int trona_estimate_charge(int variant, int temp, int voltage, int current_ua, int *full)
{
 int i, hi, lo, weight, span;
 if (temp >= profile_temps[0]) i=0;
 else if (temp <= profile_temps[4]) i=4;
 else {
  for (i=1;i<5;i++) if (temp >= profile_temps[i]) break;
  hi=trona_curve_charge(variant,i-1,voltage,current_ua);
  lo=trona_curve_charge(variant,i,voltage,current_ua);
  weight=temp-profile_temps[i]; span=profile_temps[i-1]-profile_temps[i];
  *full=profile_full_uah[variant][i]+(long long)(profile_full_uah[variant][i-1]-profile_full_uah[variant][i])*weight/span;
  return lo+(long long)(hi-lo)*weight/span;
 }
 *full=profile_full_uah[variant][i];
 return trona_curve_charge(variant,i,voltage,current_ua);
}
