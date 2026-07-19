// Build: gcc -O2 -std=c17 -Wall -Wextra mkfs_adder.c -o mkfs_adder
#define _FILE_OFFSET_BITS 64   // enables large file support (>2GB files)

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <inttypes.h>
#include <errno.h>
#include <time.h>
#include <assert.h>

#define BS 4096u           // block size
#define INODE_SIZE 128u
#define ROOT_INO 1u
#define DIRECT_MAX 12

enum FILE_TYPE { TYPE_FILE = 1, TYPE_DIR = 2 };
enum INODE_MODE { MODE_FILE = 0100000, MODE_DIR = 0040000 };

#pragma pack(push,1)
typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t block_size;
    uint64_t total_blocks;
    uint64_t inode_count;
    uint64_t inode_bitmap_start;
    uint64_t inode_bitmap_blocks;
    uint64_t data_bitmap_start;
    uint64_t data_bitmap_blocks;
    uint64_t inode_table_start;
    uint64_t inode_table_blocks;
    uint64_t data_region_start;
    uint64_t data_region_blocks;
    uint64_t root_inode;
    uint64_t mtime_epoch;
    uint32_t flags;
    uint32_t checksum;
} superblock_t;
_Static_assert(sizeof(superblock_t)==116,"sb size");

typedef struct {
    uint16_t mode;
    uint16_t links;
    uint32_t uid;
    uint32_t gid;
    uint64_t size_bytes;
    uint64_t atime;
    uint64_t mtime;
    uint64_t ctime;
    uint32_t direct[DIRECT_MAX];
    uint32_t reserved_0, reserved_1, reserved_2;
    uint32_t proj_id, uid16_gid16;
    uint64_t xattr_ptr;
    uint64_t inode_crc;
} inode_t;
_Static_assert(sizeof(inode_t)==INODE_SIZE,"inode size");

typedef struct {
    uint32_t inode_no;
    uint8_t  type;
    char     name[58];
    uint8_t  checksum;
} dirent64_t;
_Static_assert(sizeof(dirent64_t)==64,"dirent size");
#pragma pack(pop)

static uint32_t CRC32_TAB[256];

static void crc32_init(void) {
    for (uint32_t i=0;i<256;i++) {
        uint32_t c=i;
        for(int j=0;j<8;j++) c=(c&1)?(0xEDB88320u^(c>>1)):(c>>1);
        CRC32_TAB[i]=c;
    }
}
static uint32_t crc32(const void* data, size_t n) {
    const uint8_t* p=(const uint8_t*)data; uint32_t c=0xFFFFFFFFu;
    for(size_t i=0;i<n;i++) c=CRC32_TAB[(c^p[i])&0xFF]^(c>>8);
    return c^0xFFFFFFFFu;
}

// FIX 1: Make superblock checksum consistent with the builder.
// It should be calculated over the struct, not the entire block.
static uint32_t superblock_crc_finalize(superblock_t *sb) {
    sb->checksum = 0;
    uint32_t s = crc32((void *) sb, sizeof(superblock_t) - sizeof(uint32_t));
    sb->checksum = s;
    return s;
}
static void inode_crc_finalize(inode_t* ino) {
    uint8_t tmp[INODE_SIZE]; memcpy(tmp,ino,INODE_SIZE);
    memset(&tmp[120],0,8);
    uint32_t c=crc32(tmp,120);
    ino->inode_crc=(uint64_t)c;
}
static void dirent_checksum_finalize(dirent64_t* de) {
    de->checksum = 0;
    const uint8_t* p=(const uint8_t*)de; uint8_t x=0;
    for(int i=0;i<63;i++) x^=p[i];
    de->checksum=x;
}

static void set_bit(uint8_t *bm, uint64_t idx) {
    bm[idx/8] |= (uint8_t)(1u<<(idx%8));
}

static int64_t find_zero(const uint8_t *bm, uint64_t bits) {
    for(uint64_t i=0;i<bits;i++) {
        if(!(bm[i/8]&(1u<<(i%8)))) return (int64_t)i;
    }
    return -1;
}

int main(int argc, char *argv[]) {
    crc32_init();

    if(argc!=7) {
        fprintf(stderr,"Usage: %s --input <img> --output <img> --file <fname>\n",argv[0]);
        return 1;
    }
    char *in_path=NULL,*out_path=NULL,*file_name=NULL;
    for(int i=1;i<argc;i+=2){
        if(!strcmp(argv[i],"--input")) in_path=argv[i+1];
        else if(!strcmp(argv[i],"--output")) out_path=argv[i+1];
        else if(!strcmp(argv[i],"--file")) file_name=argv[i+1];
        else{fprintf(stderr,"Unknown %s\n",argv[i]);return 1;}
    }
    if(!in_path||!out_path||!file_name){
        fprintf(stderr,"Missing args\n");return 1;
    }

    FILE *in_fp=fopen(in_path,"rb");
    if(!in_fp){perror("open input");return 1;}
    fseek(in_fp,0,SEEK_END);
    long long sz=ftell(in_fp);
    if(sz<0){perror("ftell");fclose(in_fp);return 1;}
    uint64_t img_size=(uint64_t)sz;
    fseek(in_fp,0,SEEK_SET);
    uint8_t *img=(uint8_t*)malloc(img_size);
    if(!img){perror("malloc");fclose(in_fp);return 1;}
    if(fread(img,1,img_size,in_fp)!=img_size){
        perror("read img");fclose(in_fp);free(img);return 1;
    }
    fclose(in_fp);

    superblock_t *sb=(superblock_t*)img;
    uint8_t *inode_bm=img + sb->inode_bitmap_start * BS;
    uint8_t *data_bm =img + sb->data_bitmap_start  * BS;
    inode_t *it=(inode_t*)(img + sb->inode_table_start * BS);

    // FIX 2: Locate root directory data block dynamically, not by assumption.
    // This is more robust than assuming it's the first data block.
    inode_t* root_inode_ptr = &it[ROOT_INO - 1];
    uint32_t root_data_block_num = root_inode_ptr->direct[0];
    uint8_t* root_data_block_ptr = img + root_data_block_num * BS;
    dirent64_t *root_dir = (dirent64_t*)root_data_block_ptr;

    FILE *f_fp=fopen(file_name,"rb");
    if(!f_fp){perror("open file");free(img);return 1;}
    fseek(f_fp,0,SEEK_END);
    sz=ftell(f_fp);
    if(sz<0){perror("ftell file");fclose(f_fp);free(img);return 1;}
    uint64_t fsize=(uint64_t)sz;
    fseek(f_fp,0,SEEK_SET);
    uint8_t *fdata=(uint8_t*)malloc(fsize ? fsize : 1);
    if(!fdata){perror("malloc file");fclose(f_fp);free(img);return 1;}
    if(fsize && fread(fdata,1,fsize,f_fp)!=fsize){perror("read file");fclose(f_fp);free(fdata);free(img);return 1;}
    fclose(f_fp);

    uint64_t need_blocks=(fsize+BS-1)/BS;
    if(need_blocks>DIRECT_MAX){
        fprintf(stderr,"File too large (%" PRIu64 " blocks needed, max %d)\n", need_blocks, DIRECT_MAX);
        free(fdata); free(img); return 1;
    }

    int64_t ino_idx=find_zero(inode_bm, sb->inode_count);
    if(ino_idx<0){fprintf(stderr,"No free inode\n");free(fdata);free(img);return 1;}
    
    inode_t *ino=&it[ino_idx];
    memset(ino,0,sizeof(inode_t));
    ino->mode=MODE_FILE;
    ino->links=1;
    ino->size_bytes=fsize;
    time_t now=time(NULL);
    ino->atime=ino->mtime=ino->ctime=(uint64_t)now;

    for(uint64_t i=0;i<need_blocks;i++){
        int64_t db=find_zero(data_bm,sb->data_region_blocks);
        if(db<0){fprintf(stderr,"No free data block\n");free(fdata);free(img);return 1;}
        set_bit(data_bm,(uint64_t)db);
        ino->direct[i]=(uint32_t)(sb->data_region_start + (uint64_t)db);
        
        uint64_t src_off = i*BS;
        uint64_t remain = fsize > src_off ? fsize - src_off : 0;
        size_t chunk = (size_t)(remain > BS ? BS : remain);
        if(chunk > 0) {
            memcpy(img + (sb->data_region_start + (uint64_t)db)*BS, fdata + src_off, chunk);
        }
    }
    set_bit(inode_bm,(uint64_t)ino_idx);
    inode_crc_finalize(ino);

    int ent=2; // skip "." and ".."
    int max_entries = (int)(BS/sizeof(dirent64_t));
    while(ent < max_entries && root_dir[ent].inode_no) ent++;
    if(ent>=max_entries){
        fprintf(stderr,"Root dir full\n");free(fdata);free(img);return 1;
    }
    root_dir[ent].inode_no = (uint32_t)(ino_idx+1);
    root_dir[ent].type = TYPE_FILE;
    strncpy(root_dir[ent].name, file_name, sizeof(root_dir[ent].name)-1);
    dirent_checksum_finalize(&root_dir[ent]);

    // FIX 3: Update the root directory's inode metadata (links, time, checksum)
    // This was the main logical error in the original code.
    root_inode_ptr->links++;
    root_inode_ptr->mtime = now;
    root_inode_ptr->ctime = now;
    inode_crc_finalize(root_inode_ptr);

    // Update superblock mtime and finalize its checksum
    sb->mtime_epoch = now;
    superblock_crc_finalize(sb);

    FILE *out_fp=fopen(out_path,"wb");
    if(!out_fp){perror("open out");free(fdata);free(img);return 1;}
    if(fwrite(img,1,img_size,out_fp)!=img_size){
        perror("write out");fclose(out_fp);free(fdata);free(img);return 1;
    }
    fclose(out_fp);

    free(fdata);
    free(img);

    printf("Added '%s' as inode %lld\n", file_name, (long long)(ino_idx+1));
    return 0;
}