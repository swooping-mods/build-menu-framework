#ifndef MOON_BUILD_STANDALONE_UPDATE_H
#define MOON_BUILD_STANDALONE_UPDATE_H
/* Hints may already own standalone Update through Sound. Reuse that existing
 * callback service; absence keeps the original single-provider hook path. */
static void mb_sound_update(void *walker){
    if(!__atomic_load_n(&mb_started,__ATOMIC_ACQUIRE))return;
    Context context={0};context.rcx=(U64)walker;before(0,&context);
}
static int mb_sound_driver(void){
    static char path[1200];DWORD n=GetModuleFileNameA(0,path,1000);if(!n||n>=1000)return -1;
    while(n&&path[n-1]!='\\'&&path[n-1]!='/')n--;
    if(!n)return -1;
    cat(path+n,"MoonLab\\mods\\sound-framework\\mod.dll");PTR module=GetModuleHandleA(path);if(!module)return 0;
    MoonSoundsGet get=(MoonSoundsGet)GetProcAddress(module,"MoonLab_Sounds_v1");
    const MoonSounds *sounds=get?get(MA_ABI,sizeof(MoonSounds)):0;
    return sounds&&sounds->on_update&&sounds->on_update(mb_sound_update)?1:-1;
}
#endif
