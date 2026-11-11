/* A single owner integrates independently registered build definitions. */
#include "../../runtime/native/gameplay.h"
#include "../../runtime/native/services.h"
#include "runtime.h"
#include "../sound-framework/sdk/sound.h"
__declspec(dllimport) DWORD __stdcall GetModuleFileNameA(PTR,char *,DWORD);
int _fltused=0;
#include "registry.h"
#include "save_limit.h"
#include "icons.h"
static PTR mb_log,mb_world_field,mb_definition_field,mb_base_field,mb_cache_field;
static PTR mb_get_definition,mb_get_world,mb_begin,mb_instantiate,mb_alive,mb_destroy,mb_restore;
static PTR (*mb_nested)(PTR,PTR *);static const char *(*mb_class_name)(PTR);
static void (*mb_init_class)(PTR),(*mb_static_get)(PTR,void *),(*mb_static_set)(PTR,void *),(*mb_set)(PTR,PTR,void *);
static void (*mb_barrier)(PTR,PTR *,PTR);
static DRoot mb_original_catalog,mb_catalog,mb_world,mb_original_definition,mb_world_definition,mb_array;
static DWORD mb_thread;static int mb_started,mb_busy,mb_failed,mb_test;
#include "power_runtime.h"
static void mb_event(const char *name){
    if(!mb_log)return;char b[256],*p=cat(b,"{\"event\":\"");p=cat(p,name);p=cat(p,"\"}\n");DWORD n=0;WriteFile(mb_log,b,(DWORD)(p-b),&n,0);FlushFileBuffers(mb_log);
}
static PTR mb_field(PTR obj,const char *name){PTR value=0;if(obj){PTR f=get_field(object_class(obj),name);if(f)field_value(obj,f,&value);}return value;}
static int mb_int(PTR obj,const char *name){int value=0;if(obj){PTR f=get_field(object_class(obj),name);if(f)field_value(obj,f,&value);}return value;}
static int mb_scalar(PTR obj,const char *name,void *value){PTR f=get_field(object_class(obj),name);if(!f)return 0;mb_set(obj,f,value);return 1;}
static int mb_reference(PTR obj,const char *name,PTR value){
    PTR f=get_field(object_class(obj),name);if(!f)return 0;
    int (*offset)(PTR)=(int(*)(PTR))GetProcAddress(GetModuleHandleA("GameAssembly.dll"),"il2cpp_field_get_offset");
    int n=offset?offset(f):0;if(n<16)return 0;mb_barrier(obj,(PTR *)((unsigned char *)obj+n),value);return 1;
}
static int mb_unity_alive(PTR obj){int ok=1;void *a[]={obj};PTR v=obj?call(mb_alive,0,a,&ok):0;return ok&&v&&*(unsigned char *)unbox(v);}
static void mb_delete(PTR obj){if(!obj)return;int ok=1;void *a[]={obj};call(mb_destroy,0,a,&ok);}
static PTR mb_nested_class(PTR parent,const char *name){PTR it=0,k;while((k=mb_nested(parent,&it)))if(deq(mb_class_name(k),name))return k;return 0;}
static int mb_publish_catalog(void){
    if(mb_catalog)return 1;
    PTR old=0;mb_static_get(mb_base_field,&old);U64 count=old?da.array_length(old):0;
    if(!count||count>512){mb_event("invalid-native-catalog");return 0;}
    mb_original_catalog=dhold(old);if(!mb_original_catalog)return 0;
    unsigned added=0;for(int i=0;i<MB_LIMIT;i++)added+=mb_entries[i].used!=0;
    PTR klass=mb_nested_class(find_class("MoonCleaners","MoonDepotBuilder"),"CatalogEntry");
    PTR ctor=klass?get_method(klass,".ctor",4):0;
    PTR (*new_array)(PTR,U64)=(PTR(*)(PTR,U64))GetProcAddress(GetModuleHandleA("GameAssembly.dll"),"il2cpp_array_new");
    if(!ctor||method_token(ctor)!=100663936||!new_array)return 0;
    PTR array=new_array(klass,count+added);DRoot hold=dhold(array);if(!hold)return 0;
    for(U64 i=0;i<count;i++)mb_barrier(array,(PTR *)darray(array,i),*(PTR *)darray(old,i));
    U64 at=count;
    for(int kind=MB_FIRST_KIND;kind<=MB_LAST_KIND;kind++){
        MBEntry *e=mb_entry(kind);if(!e)continue;PTR donor=0;
        for(U64 i=0;i<count;i++){PTR entry=*(PTR *)darray(old,i);if(entry&&mb_int(entry,"kind")==e->def.template_kind){donor=entry;break;}}
        if(!donor){mb_event("catalog-template-missing");ddrop(&hold);return 0;}
        PTR entry=da.new_object(klass);DRoot root=dhold(entry);if(!root){ddrop(&hold);return 0;}
        PTR image=e->def.icon_png?da.string(e->def.key):e->def.image?da.string(e->def.image):mb_field(donor,"image");int ok=1,id=e->def.kind;
        void *args[]={&id,da.string(e->def.name),da.string(e->def.category),image};call(ctor,entry,args,&ok);
        if(!ok){ddrop(&root);ddrop(&hold);return 0;}
        mb_barrier(array,(PTR *)darray(array,at++),entry);ddrop(&root);
        e->caption=dhold(da.string(e->def.description));if(!e->caption){ddrop(&hold);return 0;}
    }
    PTR check=0;mb_static_get(mb_base_field,&check);
    if(check!=dget(mb_original_catalog)){mb_event("foreign-catalog-writer");ddrop(&hold);return 0;}
    mb_static_set(mb_base_field,array);mb_static_get(mb_base_field,&check);
    if(check!=array){mb_static_set(mb_base_field,old);ddrop(&hold);mb_event("catalog-publication-failed");return 0;}
    mb_catalog=hold;mb_event("build-catalog-published");return 1;
}
static void mb_world_drop(void){
    PTR world=dget(mb_world);
    if(mb_unity_alive(world)&&mb_field(world,"depotDefinition")==dget(mb_world_definition))mb_reference(world,"depotDefinition",dget(mb_original_definition));
    for(int i=0;i<MB_LIMIT;i++)if(mb_entries[i].definition){
        PTR def=dget(mb_entries[i].definition);
        if(mb_entries[i].def.release)mb_entries[i].def.release(def,world);
        mb_delete(def);ddrop(&mb_entries[i].definition);
    }
    mb_delete(dget(mb_world_definition));ddrop(&mb_world_definition);ddrop(&mb_original_definition);ddrop(&mb_array);ddrop(&mb_world);
}
static int mb_setup_world(PTR world){
    if(mb_failed||!world||mb_busy||mb_thread!=GetCurrentThreadId())return 0;
    if(world==dget(mb_world)&&mb_world_definition)return 1;
    mb_busy=1;mb_world_drop();mb_world=dhold(world);int ok=1;
    PTR base=call(mb_get_definition,world,0,&ok);if(!ok||!base)goto fail;
    mb_original_definition=dhold(base);PTR old=mb_field(base,"additionalStructures");U64 count=old?da.array_length(old):0;
    if(!count||count>MB_LAST_KIND)goto fail;
    if(mb_test){
        char b[1024],*p=cat(b,"{\"event\":\"native-build-slots\",\"count\":");p=num(p,count);p=cat(p,",\"free\":[");int comma=0;
        for(int kind=MB_FIRST_KIND;kind<=MB_LAST_KIND;kind++)if((U64)kind>count||!mb_unity_alive(*(PTR *)darray(old,kind-1))||!mb_unity_alive(mb_field(*(PTR *)darray(old,kind-1),"prefab"))){if(comma++)p=cat(p,",");p=num(p,kind);}p=cat(p,"]}\n");DWORD n=0;WriteFile(mb_log,b,(DWORD)(p-b),&n,0);FlushFileBuffers(mb_log);
        for(int kind=MB_FIRST_KIND;kind<=MB_LAST_KIND;kind++)if((U64)kind<=count){
            PTR def=*(PTR *)darray(old,kind-1),name=mb_field(def,"displayName"),prefab=mb_field(def,"prefab");
            p=cat(b,"{\"event\":\"native-build-slot\",\"kind\":");p=num(p,kind);p=cat(p,",\"prefab\":");p=num(p,mb_unity_alive(prefab));p=cat(p,",\"price\":");p=num(p,(unsigned)mb_int(def,"priceDollars"));p=cat(p,",\"named\":");p=num(p,name!=0);p=cat(p,"}\n");WriteFile(mb_log,b,(DWORD)(p-b),&n,0);
        }
    }
    U64 length=count;
    for(int i=0;i<MB_LIMIT;i++)if(mb_entries[i].used){int kind=mb_entries[i].def.kind;PTR prior=(U64)kind<=count?*(PTR *)darray(old,kind-1):0;if(mb_unity_alive(prior)&&mb_unity_alive(mb_field(prior,"prefab"))){mb_event("native-build-slot-occupied");goto fail;}if((U64)kind>length)length=(U64)kind;}
    PTR (*new_array)(PTR,U64)=(PTR(*)(PTR,U64))GetProcAddress(GetModuleHandleA("GameAssembly.dll"),"il2cpp_array_new");
    PTR array=new_array(object_class(base),length);mb_array=dhold(array);if(!mb_array)goto fail;
    for(U64 i=0;i<count;i++)mb_barrier(array,(PTR *)darray(array,i),*(PTR *)darray(old,i));
    for(int i=0;i<MB_LIMIT;i++)if(mb_entries[i].used){
        MBEntry *e=mb_entries+i;int template=e->def.template_kind;
        if((U64)template>count)goto fail;PTR donor=*(PTR *)darray(old,template-1);if(!donor)goto fail;
        void *a[]={donor};PTR def=call(mb_instantiate,0,a,&ok);e->definition=dhold(def);if(!ok||!e->definition)goto fail;
        if(!mb_reference(def,"displayName",da.string(e->def.name))||!mb_scalar(def,"priceDollars",&e->def.price))goto fail;
        if(e->def.configure&&!e->def.configure(def,world)){mb_event("provider-definition-failed");goto fail;}
        mb_barrier(array,(PTR *)darray(array,e->def.kind-1),def);
    }
    {void *a[]={base};PTR clone=call(mb_instantiate,0,a,&ok);mb_world_definition=dhold(clone);if(!ok||!mb_world_definition)goto fail;
    if(!mb_reference(clone,"additionalStructures",array)||!mb_reference(world,"depotDefinition",clone))goto fail;}
    mb_busy=0;mb_event("world-build-definitions-ready");return 1;
fail:
    mb_world_drop();mb_busy=0;mb_failed=1;mb_event("world-build-registration-failed");return 0;
}
static int mb_place(void *walker,int kind){
    if(GetCurrentThreadId()!=mb_thread||!mb_entry(kind)||mb_failed)return 0;
    int ok=1;void *a[]={dtype("MoonCleaners","MoonDepotBuilder")};
    PTR builder=call(dmethod("UnityEngine","Component","GetComponent","System.Type"),walker,a,&ok);
    void *b[]={&kind};PTR result=builder?call(mb_begin,builder,b,&ok):0;return ok&&result&&*(unsigned char *)unbox(result);
}
static int before(int id,Context *c){
    if(mb_failed)return 0;
    if(id==7){
        if(!mb_thread||mb_thread!=GetCurrentThreadId())return 0;
        for(int i=0;i<MB_LIMIT;i++)if(mb_entries[i].used&&mb_entries[i].def.icon_png&&mb_icon_key((PTR)c->rdx,mb_entries[i].def.key)){
            PTR sprite=mb_icon(mb_entries+i);if(!sprite){mb_event("provider-icon-failed");return 0;}c->rax=(U64)sprite;return 1;
        }
        return 0;
    }
    PTR world=0;
    if(id==0){
        field_value((PTR)c->rcx,mb_world_field,&world);
        if(!world){if(mb_world&&mb_thread==GetCurrentThreadId()){
            mb_world_drop();for(int i=0;i<MB_LIMIT;i++)if(mb_entries[i].used&&mb_entries[i].def.update)mb_entries[i].def.update((PTR)c->rcx);
        }return 0;}
        int ok=1;if(boolean(mb_restore,world,&ok)||!ok)return 0;
    }else if(id==1){int ok=1;world=call(mb_get_world,(PTR)c->rcx,0,&ok);if(!ok||!world)return 0;}
    else if(id==2){if(!mb_entry((int)c->rdx))return 0;world=(PTR)c->rcx;}
    else {
        MBEntry *e=mb_entry((int)(id==6?c->rdx:c->rcx));if(!e)return 0;
        if(!mb_thread||mb_thread!=GetCurrentThreadId())return 0;
        if(id==3){c->rax=e->def.power_node!=0;return 1;}
        if(id==5){
            if(!c->rdx||!e->def.power_node){c->rax=0;return 1;}
            unsigned char spec[32]={0};PTR name=dget(e->caption);dcopy(spec,&name,8);dcopy(spec+8,&e->def.peak_kw,4);dcopy(spec+12,&e->def.battery_kwh,4);dcopy(spec+16,&e->def.power_range,4);dcopy((PTR)c->rdx,spec,32);c->rax=1;return 1;
        }
        if(id==6){c->rax=(U64)dget(e->caption);return 1;}return 0;
    }
    if(!mb_thread)__atomic_store_n(&mb_thread,GetCurrentThreadId(),__ATOMIC_RELEASE);if(mb_thread!=GetCurrentThreadId())return 0;
    mb_sealed=1;
    if(!mb_catalog&&!mb_publish_catalog()){mb_failed=1;return 0;}
    if(!mb_setup_world(world))return 0;
    if(id==1){if(mb_cache_field)mb_set((PTR)c->rcx,mb_cache_field,0);return 0;}
    if(id==2){c->rax=(U64)dget(mb_entry((int)c->rdx)->definition);return 1;}
    if(id==0)for(int i=0;i<MB_LIMIT;i++)if(mb_entries[i].used&&mb_entries[i].def.update)mb_entries[i].def.update((PTR)c->rcx);
    return 0;
}
#include "standalone_update.h"
static const MoonBuildMenu mb_api={MB_ABI,sizeof(MoonBuildMenu),mb_register,mb_place};
__declspec(dllexport) const MoonBuildMenu *MoonLab_BuildMenu_v1(unsigned int abi,unsigned int size){return mb_started&&abi==MB_ABI&&size==sizeof(MoonBuildMenu)?&mb_api:0;}
__declspec(dllexport) int MoonLab_ModStart(PTR domain,PTR game){
    if(mb_started)return 1;char folder[1000],path[1200],flag[8];DWORD n=GetEnvironmentVariableA("MOONLAB_EVIDENCE",folder,1000);if(!n||n>=1000)return 0;
    cat(cat(path,folder),"\\build-menu-framework.jsonl");mb_log=CreateFileA(path,0x40000000,1,0,1,0x80,0);if(mb_log==(PTR)(U64)-1)return 0;
    mb_test=GetEnvironmentVariableA("MOONLAB_TEST_TOOLS",flag,sizeof(flag))&&flag[0]=='1';
    if(!bind_runtime(domain,game)||!dapi(game))return 0;
#define BF(F,N,T) F=(T)GetProcAddress(game,"il2cpp_" N);if(!F)return 0
    BF(mb_nested,"class_get_nested_types",PTR(*)(PTR,PTR *));BF(mb_class_name,"class_get_name",const char *(*)(PTR));
    BF(mb_init_class,"runtime_class_init",void(*)(PTR));BF(mb_static_get,"field_static_get_value",void(*)(PTR,void *));BF(mb_static_set,"field_static_set_value",void(*)(PTR,void *));
    BF(mb_set,"field_set_value",void(*)(PTR,PTR,void *));BF(mb_barrier,"gc_wbarrier_set_field",void(*)(PTR,PTR *,PTR));
#undef BF
    PTR builder=find_class("MoonCleaners","MoonDepotBuilder"),world=find_class("MoonCleaners","MoonWorld");
    int save_max=0;PTR save_class=find_class("MoonCleaners","MoonDepotSave"),save_max_field=save_class?get_field(save_class,"MaxStructureKind"):0;
    if(save_max_field)mb_static_get(save_max_field,&save_max);if(save_max!=192){mb_event("save-kind-range-mismatch");return 0;}
    PTR panel=mb_nested_class(world,"PanelSpec");
    int (*size)(PTR,unsigned *)=(int(*)(PTR,unsigned *))GetProcAddress(game,"il2cpp_class_value_size");
    int (*offset)(PTR)=(int(*)(PTR))GetProcAddress(game,"il2cpp_field_get_offset");unsigned alignment=0;
    PTR peak=panel?get_field(panel,"peakKw"):0,range=panel?get_field(panel,"range"):0;
    if(!size||!offset||!peak||!range||size(panel,&alignment)!=32||offset(peak)!=24||offset(range)!=32){mb_event("panel-layout-mismatch");return 0;}
    PTR reader=method("MoonCleaners","MoonDepotSave","Read",3);
    if(reader){char b[256],*p=cat(b,"{\"event\":\"save-reader\",\"token\":");p=num(p,method_token(reader));p=cat(p,",\"rva\":");p=num(p,(U64)*(PTR *)reader-(U64)game);p=cat(p,"}\n");DWORD n=0;WriteFile(mb_log,b,(DWORD)(p-b),&n,0);FlushFileBuffers(mb_log);}
    mb_init_class(builder);mb_base_field=get_field(builder,"baseStructures");mb_cache_field=get_field(builder,"structureEntries");mb_world_field=get_field(find_class("MoonCleaners","MoonWalker"),"world");mb_definition_field=get_field(world,"depotDefinition");
    mb_get_definition=dmethod("MoonCleaners","MoonWorld","get_DepotDefinition","");mb_get_world=dmethod("MoonCleaners","MoonDepotBuilder","get_World","");mb_begin=dmethod("MoonCleaners","MoonDepotBuilder","BeginPlacementFromShop","System.Int32");
    mb_instantiate=dmethod("UnityEngine","Object","Instantiate","UnityEngine.Object");mb_alive=dmethod("UnityEngine","Object","op_Implicit","UnityEngine.Object");mb_destroy=dmethod("UnityEngine","Object","Destroy","UnityEngine.Object");mb_restore=dmethod("MoonCleaners","MoonWorld","get_IsRestoringRendering","");
    (void)integer;(void)dzero;(void)dnumber;(void)darray;(void)dvector;
    if(!mb_base_field||!mb_cache_field||!mb_world_field||!mb_definition_field||!mb_get_definition||!mb_get_world||!mb_begin||!mb_instantiate||!mb_alive||!mb_destroy||!mb_restore){mb_event("framework-bind-failed");return 0;}
    int shared_update=moon_runtime_loader_present()?0:mb_sound_driver();
    if(shared_update<0){mb_event("standalone-update-driver-rejected");return 0;}
    if(shared_update)mb_event("standalone-sound-update-driver");
    if((!shared_update&&!prepare_hook(method("MoonCleaners","MoonWalker","Update",0),100671733,0,game))||!prepare_hook(method("MoonCleaners","MoonDepotBuilder","BuildCatalog",0),100663785,1,game)||!prepare_hook(method("MoonCleaners","MoonWorld","BuildDefinition",1),100664704,2,game)||!prepare_hook(method("MoonCleaners","MoonWorld","IsPowerNode",1),100665465,3,game)||!prepare_hook(method("MoonCleaners","MoonWorld","TryPanel",2),100665447,5,game)||!prepare_hook(method("MoonCleaners","MoonDepotBuilder","ProductionNote",1),100663802,6,game)){mb_event("framework-hook-shape-failed");return 0;}
    if(!prepare_hook(method("MoonCleaners","MoonDepotBuilder","CardArt",1),100663789,7,game)){mb_event("icon-hook-shape-failed");return 0;}
    before_hook=before;
    if(!mb_power_bind(game)||!mb_power_install(game)){mb_event("native-power-shape-mismatch");return 0;}
    if(!mb_extend_save_limit(game)){mb_restore_save_limit();mb_power_remove();mb_event("save-reader-shape-mismatch");return 0;}
    if(!install_hooks()){mb_restore_save_limit();mb_power_remove();return 0;}if(moon_runtime_loader_present()){
        const MoonRuntimeApiV1 *runtime=moon_runtime_connect();
        const MoonServiceLimitV1 limits[]={{"max_items",MB_LIMIT},{"build_menu_max_kind",MB_LAST_KIND},{"native_power_nodes",1}};
        MoonServicePublicationLimitsV1 publication={.base={.size=sizeof(publication),.service_id="build-menu",.abi=MB_ABI,.api_size=sizeof(mb_api),.api=&mb_api},.limit_count=sizeof(limits)/sizeof(limits[0]),.reserved=0,.limits=limits};
        if(!runtime||!runtime->publish(&publication.base)){mb_restore_save_limit();mb_power_remove();return 0;}
    }
    __atomic_store_n(&mb_started,1,__ATOMIC_RELEASE);mb_event("ready");return 1;
}
