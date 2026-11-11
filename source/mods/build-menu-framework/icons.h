/* Lazy sprites are provider-owned PNGs, resolved only by the exact catalog key.
 * The game's card/detail/HUD paths all retain their native CardArt call. */
static int mb_icon_key(PTR text,const char *key){
    PTR game=GetModuleHandleA("GameAssembly.dll");
    int (*length)(PTR)=(int(*)(PTR))GetProcAddress(game,"il2cpp_string_length");
    const unsigned short *(*chars)(PTR)=(const unsigned short *(*)(PTR))GetProcAddress(game,"il2cpp_string_chars");
    if(!text||!length||!chars)return 0;int n=length(text);if(n<1||n>96)return 0;
    const unsigned short *s=chars(text);for(int i=0;i<n;i++)if(!key[i]||s[i]!=(unsigned char)key[i])return 0;return !key[n];
}
static PTR mb_icon(MBEntry *e){
    if(e->sprite)return dget(e->sprite);
    PTR game=GetModuleHandleA("GameAssembly.dll");int ok=1,size=2,format=4;unsigned char mip=1,linear=0;
    PTR (*array_new)(PTR,U64)=(PTR(*)(PTR,U64))GetProcAddress(game,"il2cpp_array_new");
    PTR texture=da.new_object(find_class("UnityEngine","Texture2D"));if(!texture||!array_new)return 0;
    DRoot root=dhold(texture);if(!root)return 0;
    void *ctor[]={&size,&size,&format,&mip,&linear};
    call(dmethod("UnityEngine","Texture2D",".ctor","System.Int32;System.Int32;UnityEngine.TextureFormat;System.Boolean;System.Boolean"),texture,ctor,&ok);
    PTR bytes=array_new(find_class("System","Byte"),e->def.icon_size);DRoot data=dhold(bytes);if(!data){ddrop(&root);return 0;}
    dcopy((unsigned char *)bytes+32,e->def.icon_png,e->def.icon_size);void *load[]={texture,bytes};
    PTR loaded=call(dmethod("UnityEngine","ImageConversion","LoadImage","UnityEngine.Texture2D;System.Byte[]"),0,load,&ok);ddrop(&data);
    if(!ok||!loaded||!*(unsigned char *)unbox(loaded)){ddrop(&root);return 0;}
    PTR create=dmethod("UnityEngine","Sprite","Create","UnityEngine.Texture2D;UnityEngine.Rect;UnityEngine.Vector2;System.Single");
    if(!create)create=dmethod("UnityEngine","Sprite","Create","UnityEngine.Texture2D;UnityEngine.Rect;UnityEngine.Vector2;System.Single;System.UInt32;UnityEngine.SpriteMeshType;UnityEngine.Vector4;System.Boolean");
    float rect[]={0,0,(float)e->def.icon_width,(float)e->def.icon_height},pivot[]={.5f,.5f},ppu=100,border[4]={0};unsigned extrude=0;int mesh=1;unsigned char physics=0;
    void *args[]={texture,rect,pivot,&ppu,&extrude,&mesh,border,&physics};PTR sprite=create?call(create,0,args,&ok):0;
    if(!ok||!sprite){ddrop(&root);return 0;}e->sprite=dhold(sprite);if(!e->sprite){ddrop(&root);return 0;}e->texture=root;return sprite;
}
