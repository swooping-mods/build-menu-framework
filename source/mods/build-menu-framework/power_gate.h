#ifndef MOON_BUILD_POWER_GATE_H
#define MOON_BUILD_POWER_GATE_H
#include "../../runtime/sdk/context.h"

/* The shipping IL2CPP solver inlines these predicates. Hooking IsPowerNode or
 * Joined alone therefore never admits a newly registered structure. These
 * continuations reproduce only the overwritten branch/SETLE instruction. */
typedef struct {
    unsigned rva,resume,reject;
    unsigned char length,condition,kind_register,bytes[6];
} MBPowerGate;
enum {MB_POWER_JNE,MB_POWER_JA,MB_POWER_SETLE};
enum {MB_POWER_R14,MB_POWER_R12,MB_POWER_RBX};
static const MBPowerGate mb_power_gates[]={
    {0x7450bc,0x7450c2,0x745030,6,MB_POWER_JNE,MB_POWER_R14,{0x0f,0x85,0x6e,0xff,0xff,0xff}},
    {0x745334,0x745337,0,3,MB_POWER_SETLE,MB_POWER_R12,{0x0f,0x9e,0xc0}},
    {0x715b8e,0x715b94,0x715d73,6,MB_POWER_JA,MB_POWER_R14,{0x0f,0x87,0xdf,0x01,0,0}},
    {0x715e80,0x715e86,0x71608e,6,MB_POWER_JA,MB_POWER_RBX,{0x0f,0x87,0x08,0x02,0,0}},
    {0x715f06,0x715f09,0,3,MB_POWER_SETLE,MB_POWER_RBX,{0x0f,0x9e,0xc0}}
};
#define MB_POWER_GATE_COUNT (sizeof(mb_power_gates)/sizeof(mb_power_gates[0]))
static int mb_power_kind(const MBPowerGate *gate,const MoonHookContext *c){
    return (int)(unsigned)(gate->kind_register==MB_POWER_R14?c->r14:gate->kind_register==MB_POWER_R12?c->r12:c->rbx);
}
static int mb_power_gate_matches(const MBPowerGate *gate,const unsigned char *bytes){
    for(unsigned i=0;i<gate->length;i++)if(bytes[i]!=gate->bytes[i])return 0;
    return 1;
}
static void mb_power_continue(const MBPowerGate *gate,MoonHookContext *c,unsigned long long base,int registered_power){
    unsigned flags=c->eflags;int zero=(flags&0x40)!=0;
    if(gate->condition==MB_POWER_SETLE){
        int sign=(flags&0x80)!=0,overflow=(flags&0x800)!=0;
        c->rax=(c->rax&~255ull)|(unsigned)(registered_power||zero||(sign!=overflow));
        c->rip=base+gate->resume;
    }else{
        int rejected=gate->condition==MB_POWER_JNE?!zero:!(flags&1)&&!zero;
        c->rip=base+((!registered_power&&rejected)?gate->reject:gate->resume);
    }
}
#endif
