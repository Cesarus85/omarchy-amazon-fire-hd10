#!/usr/bin/env python3
"""Execute the real EAPOL statistics function with optional RX metadata absent."""
from pathlib import Path
import subprocess, tempfile, resource, sys
resource.setrlimit(resource.RLIMIT_CORE,(0,0))
source=Path(sys.argv[1]) if len(sys.argv)>1 else Path(__file__).resolve().parents[1]/'wlan/core/gen3/mgmt/privacy.c'
s=source.read_text();start=s.index('void secHandleRxEapolPacket('); body=s[start:s.index('#endif',start)]
pre=r'''
#include <stdint.h>
#include <stddef.h>
#include <assert.h>
#define IN
#define FALSE 0
#define ETHER_HEADER_LEN 14
#define MASK_FC_RETRY 0x800
#define DBGLOG(...)
typedef uint16_t UINT_16;
struct group { unsigned rcpi; uint16_t fc; };
struct hdr { uint16_t u2FrameCtrl; };
typedef struct hdr *P_WLAN_MAC_HEADER_T;
typedef struct group *P_HW_MAC_RX_STS_GROUP_4_T;
struct rfb { unsigned u2PacketLen; int *prRxStatus; void *pvHeader; struct group *prRxStatusGroup4,*prRxStatusGroup3; };
typedef struct rfb *P_SW_RFB_T;
struct adapter { struct { unsigned ucM1Rcpi,ucM1Retransmission; } arRxRssiStatistics; };
typedef struct adapter *P_ADAPTER_T;
#define HAL_RX_STATUS_IS_HEADER_TRAN(p) (*(p))
#define HAL_RX_STATUS_GET_FRAME_CTL_FIELD(p) ((p)->fc)
#define HAL_RX_STATUS_GET_RCPI(p) ((p)->rcpi)
'''
post=r'''
int main(void) {
 struct adapter a = { .arRxRssiStatistics = {77, 0} };
 int translated=1; struct hdr h={0x800}; struct group g3={92,0},g4={0,0x800};
 struct rfb r={64,&translated,&h,&g4,0};
 secHandleRxEapolPacket(&a,&r);
 assert(a.arRxRssiStatistics.ucM1Rcpi==77);
 assert(a.arRxRssiStatistics.ucM1Retransmission==0x800);
 r.prRxStatusGroup3=&g3; secHandleRxEapolPacket(&a,&r);
 assert(a.arRxRssiStatistics.ucM1Rcpi==92);
 r.prRxStatusGroup4=0; secHandleRxEapolPacket(&a,&r);
 translated=0; r.prRxStatusGroup3=0; secHandleRxEapolPacket(&a,&r);
 assert(a.arRxRssiStatistics.ucM1Rcpi==92);
 return 0;
}
'''
with tempfile.TemporaryDirectory() as temp:
 c=Path(temp)/'test.c';exe=Path(temp)/'test';c.write_text(pre+body+post)
 subprocess.run(['cc','-Wall','-Werror','-fsanitize=undefined,address',str(c),'-o',str(exe)],check=True)
 result=subprocess.run([str(exe)],capture_output=True,text=True,timeout=10)
 if result.returncode: print(result.stderr[:1600],file=sys.stderr);raise SystemExit(1)
print('PASS: absent group3, present group3, missing group4, untranslated frame')
