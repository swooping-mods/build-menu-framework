#ifndef MOON_BUILD_POWER_RUNTIME_H
#define MOON_BUILD_POWER_RUNTIME_H
#include "power_gate.h"
#include "power_sites.h"
static unsigned char *mb_power_image;
static PTR mb_power_handler;
static unsigned mb_power_modified,mb_power_flush,mb_power_protection;
static DWORD mb_power_original_protection[MB_POWER_GATE_COUNT];
/* A pending CAS publishes its result before a racing trap dispatches. Restored
 * sites serve already raised exceptions but never claim a foreign INT3. */
static int mb_power_state[MB_POWER_GATE_COUNT];
static unsigned long long mb_power_hits[MB_POWER_GATE_COUNT];
static int mb_power_registered(int kind){
    if(kind<MB_FIRST_KIND||kind>MB_LAST_KIND)return 0;
    if(!__atomic_load_n(&mb_started,__ATOMIC_ACQUIRE)
       ||__atomic_load_n(&mb_thread,__ATOMIC_ACQUIRE)!=GetCurrentThreadId())return 0;
    return !mb_failed&&mb_power_kinds[kind-MB_FIRST_KIND];
}
static int mb_power_exception(ExceptionPointers *e){
    if(e->record->code!=0x80000003)return 0;
    for(unsigned i=0;i<MB_POWER_GATE_COUNT;i++){
        const MBPowerGate *gate=mb_power_gates+i;
        if(e->record->address!=(PTR)(mb_power_image+gate->rva))continue;
        int state;do{state=__atomic_load_n(mb_power_state+i,__ATOMIC_ACQUIRE);}while(state==1);
        if(state!=2&&(state!=3||mb_power_image[gate->rva]==0xcc))return 0;
        int power=state==2&&mb_power_registered(mb_power_kind(gate,e->context));
        if(power&&mb_test)mb_power_hits[i]++;
        mb_power_continue(gate,e->context,(U64)mb_power_image,power);return -1;
    }
    return 0;
}
static int mb_power_shape(PTR game){
    for(unsigned i=0;i<MB_POWER_GATE_COUNT;i++){
        if(!mb_power_gate_matches(mb_power_gates+i,(unsigned char *)game+mb_power_gates[i].rva))return 0;
        const unsigned char *p=(unsigned char *)game+mb_power_gates[i].rva-16;
        for(unsigned j=0;j<32;j++)if(p[j]!=mb_power_guards[i][j])return 0;
    }
    return 1;
}
static int mb_power_bind(PTR game){
    for(unsigned i=0;i<sizeof(mb_power_methods)/sizeof(mb_power_methods[0]);i++){
        const MBPowerMethod *m=mb_power_methods+i;PTR value=method("MoonCleaners","MoonWorld",m->name,(int)m->arity);
        if(!value||method_token(value)!=m->token||*(unsigned char **)value!=(unsigned char *)game+m->rva)return 0;
        U64 hash=14695981039346656037ull;const unsigned char *p=*(unsigned char **)value;
        for(unsigned j=0;j<m->size;j++)hash=(hash^p[j])*1099511628211ull;
        if(hash!=m->hash)return 0;
    }
    return mb_power_shape(game);
}
static int mb_power_remove(void){
    int complete=1;
    for(unsigned i=0;i<MB_POWER_GATE_COUNT;i++){
        unsigned bit=1u<<i;unsigned char *p=mb_power_image+mb_power_gates[i].rva;DWORD ignored=0;
        if(mb_power_modified&bit){
            if(!VirtualProtect(p,1,0x40,&ignored)){complete=0;continue;}
            mb_power_protection|=bit;
            unsigned char expected=0xcc;
            if(!__atomic_compare_exchange_n(p,&expected,mb_power_gates[i].bytes[0],0,__ATOMIC_SEQ_CST,__ATOMIC_SEQ_CST)){complete=0;}
            else{mb_power_modified&=~bit;mb_power_flush|=bit;__atomic_store_n(mb_power_state+i,3,__ATOMIC_RELEASE);}
        }
        if(mb_power_flush&bit){if(FlushInstructionCache(GetCurrentProcess(),p,1))mb_power_flush&=~bit;else complete=0;}
        if(mb_power_protection&bit){if(VirtualProtect(p,1,mb_power_original_protection[i],&ignored))mb_power_protection&=~bit;else complete=0;}
    }
    /* A different thread may already have raised an owned breakpoint before
     * rollback. Retain its pass-through handler for this resident DLL lifetime. */
    return complete&&!mb_power_modified&&!mb_power_flush&&!mb_power_protection;
}
static int mb_power_install(PTR game){
    if(mb_power_handler||!mb_power_shape(game))return 0;
    mb_power_image=game;mb_power_handler=AddVectoredExceptionHandler(1,mb_power_exception);if(!mb_power_handler)return 0;
    for(unsigned i=0;i<MB_POWER_GATE_COUNT;i++){
        unsigned bit=1u<<i;unsigned char *p=mb_power_image+mb_power_gates[i].rva;DWORD ignored=0;
        if(!VirtualProtect(p,1,0x40,mb_power_original_protection+i))goto fail;
        mb_power_protection|=bit;
        unsigned char expected=mb_power_gates[i].bytes[0];
        __atomic_store_n(mb_power_state+i,1,__ATOMIC_RELEASE);
        int changed=__atomic_compare_exchange_n(p,&expected,0xcc,0,__ATOMIC_SEQ_CST,__ATOMIC_SEQ_CST);
        __atomic_store_n(mb_power_state+i,changed?2:0,__ATOMIC_RELEASE);
        if(changed){mb_power_modified|=bit;mb_power_flush|=bit;}
        int flushed=changed&&FlushInstructionCache(GetCurrentProcess(),p,1);if(flushed)mb_power_flush&=~bit;
        int protected=VirtualProtect(p,1,mb_power_original_protection[i],&ignored);if(protected)mb_power_protection&=~bit;
        if(!changed||!flushed||!protected)goto fail;
    }
    return 1;
fail:
    mb_power_remove();return 0;
}
#endif
