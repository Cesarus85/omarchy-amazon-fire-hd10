/* SPDX-License-Identifier: MIT
 * Experimental Cortex-M4 decoder MMIO proxy. No security-controller writes.
 */
typedef unsigned int u32;
typedef unsigned short u16;
#define REG(a) (*(volatile u32 *)(a))
struct mailbox {u32 magic,req,done,addr,value,write,width,error,pc,count,timeout,old_ctrl,old_rasr,old_vtor,ready;};
#define M ((volatile struct mailbox *)0x57000)
static u32 getreg(u32 *f,u32 *hi,u32 r) {
 if(r<4)return f[r]; if(r<12)return hi[r-4]; if(r==12)return f[4]; if(r==14)return f[5];return 0;
}
static void putreg(u32 *f,u32 *hi,u32 r,u32 v) {
 if(r<4)f[r]=v;else if(r<12)hi[r-4]=v;else if(r==12)f[4]=v;else if(r==14)f[5]=v;
}
void proxy_init(void) {
 if(M->ready==1)return;
 M->magic=0x50525859; M->old_ctrl=REG(0xe000ed94); M->old_vtor=REG(0xe000ed08);
 if(((REG(0xe000ed90)>>8)&255)<8 || M->old_vtor!=0){M->error=1;return;}
 REG(0xe000ed98)=7; M->old_rasr=REG(0xe000eda0);
 if(M->old_rasr&1){M->error=2;return;}
 REG(0xe000ed9c)=0xa6000000;
 REG(0xe000eda0)=(1u<<28)|(19u<<1)|1; /* 1 MiB, no access, XN */
 REG(0xe000ed24)|=1u<<16;
 REG(0xe000ed94)=M->old_ctrl|5;
 __asm__ volatile("dsb; isb" ::: "memory");
 M->ready=1;
}
int proxy_fault(u32 *f,u32 *hi) {
 u32 cfsr=REG(0xe000ed28),a=REG(0xe000ed34),pc=f[6],rt=0,wr=0,len=0,rn=0,wb=0,newbase=0;
 if(M->magic!=0x50525859 || M->ready!=1 || (cfsr&0x82)!=0x82)return 0;
 if(!((a>=0xa6000000 && a<0xa6000100)||(a>=0xa6020000 && a<0xa6025000)) || (a&3)){M->error=3;return 0;}
 const u16 *code=(const u16 *)pc; u16 x=code[0],y=code[1];
 if((x&0xf000)==0x6000){rt=x&7;wr=!(x&0x800);len=2;}
 else if((x&0xf600)==0x5000){rt=x&7;wr=!(x&0x800);len=2;}
 else if((x&0xffe0)==0xf8c0){rt=y>>12;wr=!(x&0x10);len=4;}
 else if((x&0xffe0)==0xf840){
  rt=y>>12;wr=!(x&0x10);len=4;rn=x&15;
  if(y&0x800){wb=(y>>8)&1;newbase=getreg(f,hi,rn);if(y&0x200)newbase+=y&255;else newbase-=y&255;}
 }
 if(!len || rt==13 || rt==15 || (wb&&(rn==13||rn==15))){M->pc=pc;M->error=4;return 0;}
 M->addr=a-0x90000000;M->pc=pc;M->write=wr;M->width=4;
 if(wr)M->value=getreg(f,hi,rt);
 __asm__ volatile("dmb" ::: "memory");
 u32 q=M->req+1;M->req=q;
 for(u32 t=0;M->done!=q;t++){if(t==20000000){M->timeout++;M->error=5;return 0;}}
 __asm__ volatile("dmb" ::: "memory");
 if(M->error)return 0;
 if(!wr)putreg(f,hi,rt,M->value);
 if(wb)putreg(f,hi,rn,newbase);
 f[6]+=len;
 u32 it=((f[7]>>25)&3)|((f[7]>>8)&0xfc);
 if(it){it=(it&7)?((it&0xe0)|((it<<1)&0x1f)):0;f[7]=(f[7]&~0x0600fc00u)|((it&3)<<25)|((it&0xfc)<<8);}
 REG(0xe000ed28)=0x82;REG(0xe000ed2c)=1u<<30;
 M->count++;
 return 1;
}
