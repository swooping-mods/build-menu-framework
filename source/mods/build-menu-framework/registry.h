#ifndef MOON_BUILD_MENU_REGISTRY_H
#define MOON_BUILD_MENU_REGISTRY_H
#include "sdk/build_menu.h"
typedef struct {MoonBuildDefinition def;DRoot definition,caption,texture,sprite;int used;} MBEntry;
static MBEntry mb_entries[MB_LIMIT];
/* Native inline admission reads a fixed saved-kind index, without reflection,
 * allocation, or a registry scan for every candidate in the power solver. */
static unsigned char mb_power_kinds[MB_LAST_KIND-MB_FIRST_KIND+1];
static int mb_sealed;
static int mb_text(const char *s,int limit){if(!s||!*s)return 0;for(int i=0;i<limit;i++)if(!s[i])return 1;return 0;}
static MBEntry *mb_entry(int kind){if(kind<MB_FIRST_KIND||kind>MB_LAST_KIND)return 0;for(int i=0;i<MB_LIMIT;i++)if(mb_entries[i].used&&mb_entries[i].def.kind==kind)return mb_entries+i;return 0;}
static int mb_validate(const MoonBuildDefinition *d){
    if(!d||d->abi!=MB_ABI||(d->size!=sizeof(*d)&&d->size!=MB_LEGACY_SIZE)||mb_sealed)return 0;
    if(d->kind<MB_FIRST_KIND||d->kind>MB_LAST_KIND||d->template_kind<1||d->template_kind>=MB_FIRST_KIND||d->price<0||d->price>10000000)return 0;
    if(!mb_text(d->key,96)||!mb_text(d->name,256)||!mb_text(d->category,128)||!mb_text(d->description,1024)||(d->image&&!mb_text(d->image,256)))return 0;
    for(const char *p=d->key;*p;p++)if(!((*p>='a'&&*p<='z')||(*p>='0'&&*p<='9')||*p=='-'||*p=='.'))return 0;
    if(d->power_node<0||d->power_node>1||!(d->power_range>=0&&d->power_range<=1000)||!(d->peak_kw>=0&&d->peak_kw<=1000000)||!(d->battery_kwh>=0&&d->battery_kwh<=1000000))return 0;
    if(d->size==sizeof(*d)){
        if(d->icon_png){if(d->icon_size<24||d->icon_size>16*1024*1024||!d->icon_width||!d->icon_height||d->icon_width>4096||d->icon_height>4096)return 0;}
        else if(d->icon_size||d->icon_width||d->icon_height)return 0;
    }
    for(int i=0;i<MB_LIMIT;i++)if(mb_entries[i].used&&(mb_entries[i].def.kind==d->kind||deq(mb_entries[i].def.key,d->key)))return 0;
    return 1;
}
static int mb_register(const MoonBuildDefinition *d){
    if(!mb_validate(d))return 0;
    for(int i=0;i<MB_LIMIT;i++)if(!mb_entries[i].used){/* Copy only the advertised prefix; legacy consumers have no extension. */
        unsigned char *to=(unsigned char *)&mb_entries[i].def;const unsigned char *from=(const unsigned char *)d;
        for(unsigned n=0;n<d->size;n++)to[n]=from[n];
        mb_power_kinds[d->kind-MB_FIRST_KIND]=(unsigned char)d->power_node;
        mb_entries[i].used=1;return 1;}
    return 0;
}
#endif
