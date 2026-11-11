#ifndef MOON_BUILD_SAVE_LIMIT_H
#define MOON_BUILD_SAVE_LIMIT_H
/* One framework owns this pinned reader instruction. Its aligned first eight
 * bytes cover the complete imm32; a CAS publishes 384 without a torn immediate
 * and refuses to overwrite another writer's instruction. */
#define MB_SAVE_READER_RVA 0x326780u
#define MB_SAVE_GUARD_OFFSET 0x5d0u
typedef unsigned long long MBSaveWord;
_Static_assert(sizeof(MBSaveWord)==8,"The save patch requires a 64-bit word");
_Static_assert(__atomic_always_lock_free(8,0),"The save patch must be lock-free");
static unsigned char mb_save_byte(unsigned i,unsigned limit){
    static const unsigned char original[]={0x3d,0xc0,0,0,0,0x0f,0x87,0x84,1,0,0};
    return i>=1&&i<=4?(unsigned char)(limit>>(8*(i-1))):original[i];
}
static int mb_save_guard_limit(const unsigned char *p,unsigned limit){
    if(!p)return 0;
    for(unsigned i=0;i<11;i++)if(p[i]!=mb_save_byte(i,limit))return 0;
    return 1;
}
static int mb_save_guard_matches(const unsigned char *p){return mb_save_guard_limit(p,MB_NATIVE_LAST_KIND);}
static MBSaveWord mb_save_word(unsigned limit){
    MBSaveWord value=0;for(unsigned i=0;i<8;i++)value|=(MBSaveWord)mb_save_byte(i,limit)<<(8*i);return value;
}
static int mb_save_replace(unsigned char *p,unsigned from,unsigned to){
    if(!p||((MBSaveWord)p&7)||!mb_save_guard_limit(p,from))return 0;
    MBSaveWord expected=mb_save_word(from);
    if(!__atomic_compare_exchange_n((MBSaveWord *)(void *)p,&expected,mb_save_word(to),0,__ATOMIC_SEQ_CST,__ATOMIC_SEQ_CST))return 0;
    return mb_save_guard_limit(p,to);
}
#ifndef MB_SAVE_TEST_ONLY
static unsigned char *mb_save_guard;
static DWORD mb_save_protection;
static int mb_save_protection_pending;
static int mb_save_close_protection(void){
    if(!mb_save_protection_pending)return 1;
    DWORD ignored=0;if(!VirtualProtect(mb_save_guard,8,mb_save_protection,&ignored))return 0;
    mb_save_protection_pending=0;return 1;
}
static int mb_restore_save_limit(void){
    if(!mb_save_guard)return 1;
    if(mb_save_guard_matches(mb_save_guard)){
        int ok=mb_save_close_protection();if(ok)mb_save_guard=0;return ok;
    }
    if(!mb_save_guard_limit(mb_save_guard,MB_LAST_KIND)){mb_save_close_protection();return 0;}
    if(!mb_save_protection_pending){
        if(!VirtualProtect(mb_save_guard,8,0x40,&mb_save_protection))return 0;
        mb_save_protection_pending=1;
    }
    int changed=mb_save_replace(mb_save_guard,MB_LAST_KIND,MB_NATIVE_LAST_KIND);
    int flushed=changed&&FlushInstructionCache(GetCurrentProcess(),mb_save_guard,11);
    int restored=mb_save_close_protection();
    if(changed&&flushed&&restored){mb_save_guard=0;return 1;}return 0;
}
static int mb_extend_save_limit(PTR game){
    PTR reader=method("MoonCleaners","MoonDepotSave","Read",3);
    if(!reader||method_token(reader)!=100666517||*(unsigned char **)reader!=(unsigned char *)game+MB_SAVE_READER_RVA)return 0;
    unsigned char *guard=*(unsigned char **)reader+MB_SAVE_GUARD_OFFSET;
    if(((MBSaveWord)guard&7)||!mb_save_guard_matches(guard))return 0;
    if(!VirtualProtect(guard,8,0x40,&mb_save_protection))return 0;
    mb_save_guard=guard;mb_save_protection_pending=1;
    int changed=mb_save_replace(guard,MB_NATIVE_LAST_KIND,MB_LAST_KIND);
    int flushed=changed&&FlushInstructionCache(GetCurrentProcess(),guard,11);
    if(changed&&!flushed){mb_save_replace(guard,MB_LAST_KIND,MB_NATIVE_LAST_KIND);FlushInstructionCache(GetCurrentProcess(),guard,11);}
    int restored=mb_save_close_protection();
    return changed&&flushed&&restored&&mb_save_guard_limit(guard,MB_LAST_KIND);
}
#endif
#endif
