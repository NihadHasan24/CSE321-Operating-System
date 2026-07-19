// Build: gcc -O2 -std=c17 -Wall -Wextra mkfs_builder_with_seed.c -o mkfs_builder
#define _FILE_OFFSET_BITS 64 // enables large file support(>2GB files)
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <inttypes.h>
#include <errno.h>
#include <time.h>
#include <assert.h>

#define BS 4096u          // block size
#define INODE_SIZE 128u
#define ROOT_INO 1u
#define DIRECT_MAX 12

// NEW: The global seed variable from the template, now properly used.
uint64_t g_random_seed = 0;

// Define file type enum for directory entries
enum FILE_TYPE {
    TYPE_FILE = 1,
    TYPE_DIR = 2,
};

// Define inode mode enum
enum INODE_MODE {
    MODE_FILE = 0100000, // Octal representation for file
    MODE_DIR  = 0040000,  // Octal representation for directory
};

#pragma pack(push, 1)
typedef struct {
    uint32_t magic;           // Magic number 0x4D565346 ("MVSF")
    uint32_t version;         // Version 1
    uint32_t block_size;      // Block size (4096)
    uint64_t total_blocks;    // Total blocks in file system
    uint64_t inode_count;     // Total number of inodes
    uint64_t inode_bitmap_start; // Starting block of inode bitmap
    uint64_t inode_bitmap_blocks; // Number of inode bitmap blocks (1)
    uint64_t data_bitmap_start;  // Starting block of data bitmap
    uint64_t data_bitmap_blocks; // Number of data bitmap blocks (1)
    uint64_t inode_table_start;  // Starting block of inode table
    uint64_t inode_table_blocks; // Number of inode table blocks
    uint64_t data_region_start;  // Starting block of data region
    uint64_t data_region_blocks; // Number of data blocks
    uint64_t root_inode;      // Root inode number (1)
    uint64_t mtime_epoch;     // Build time (Unix Epoch)
    uint32_t flags;           // Flags (0)
    
    // THIS FIELD SHOULD STAY AT THE END
    // ALL OTHER FIELDS SHOULD BE ABOVE THIS
    uint32_t checksum;        // crc32 of the struct up to this point
} superblock_t;
#pragma pack(pop)
_Static_assert(sizeof(superblock_t) == 116, "superblock must fit in one block");

#pragma pack(push,1)
typedef struct {
    uint16_t mode;            // File type and permissions
    uint16_t links;           // Number of hard links
    uint32_t uid;             // User ID
    uint32_t gid;             // Group ID
    uint64_t size_bytes;      // File size in bytes
    uint64_t atime;           // Access time (Unix Epoch)
    uint64_t mtime;           // Modification time (Unix Epoch)
    uint64_t ctime;           // Creation time (Unix Epoch)
    uint32_t direct[DIRECT_MAX]; // Direct block pointers (12 blocks)
    uint32_t reserved_0;      // Reserved field 0
    uint32_t reserved_1;      // Reserved field 1
    uint32_t reserved_2;      // Reserved field 2
    uint32_t proj_id;         // Project ID (group ID)
    uint32_t uid16_gid16;     // Extended uid/gid (0)
    uint64_t xattr_ptr;       // Extended attributes pointer (0)

    // THIS FIELD SHOULD STAY AT THE END
    // ALL OTHER FIELDS SHOULD BE ABOVE THIS
    uint64_t inode_crc; // low 4 bytes store crc32 of bytes [0..119]; high 4 bytes 0
} inode_t;
#pragma pack(pop)
_Static_assert(sizeof(inode_t)==INODE_SIZE, "inode size mismatch");

#pragma pack(push,1)
typedef struct {
    uint32_t inode_no;        // Inode number (0 if free)
    uint8_t  type;            // File type (1=file, 2=dir)
    char     name[58];        // Filename (null-terminated)
    
    uint8_t  checksum; // XOR of bytes 0..62
} dirent64_t;
#pragma pack(pop)
_Static_assert(sizeof(dirent64_t)==64, "dirent size mismatch");

// ==========================DO NOT CHANGE THIS PORTION=========================
uint32_t CRC32_TAB[256];
void crc32_init(void){
    for (uint32_t i=0;i<256;i++){
        uint32_t c=i;
        for(int j=0;j<8;j++) c = (c&1)?(0xEDB88320u^(c>>1)):(c>>1);
        CRC32_TAB[i]=c;
    }
}
uint32_t crc32(const void* data, size_t n){
    const uint8_t* p=(const uint8_t*)data; uint32_t c=0xFFFFFFFFu;
    for(size_t i=0;i<n;i++) c = CRC32_TAB[(c^p[i])&0xFF] ^ (c>>8);
    return c ^ 0xFFFFFFFFu;
}
// ====================================CRC32====================================

static uint32_t superblock_crc_finalize(superblock_t *sb) {
    sb->checksum = 0;
    uint32_t s = crc32((void *) sb, sizeof(superblock_t) - sizeof(uint32_t));
    sb->checksum = s;
    return s;
}

void inode_crc_finalize(inode_t* ino){
    uint8_t tmp[INODE_SIZE]; memcpy(tmp, ino, INODE_SIZE);
    memset(&tmp[120], 0, 8);
    uint32_t c = crc32(tmp, 120);
    ino->inode_crc = (uint64_t)c;
}

void dirent_checksum_finalize(dirent64_t* de) {
    de->checksum = 0;
    const uint8_t* p = (const uint8_t*)de;
    uint8_t x = 0;
    for (int i = 0; i < 63; i++) x ^= p[i];
    de->checksum = x;
}

void set_bit(uint8_t* bitmap, uint64_t index) {
    bitmap[index / 8] |= (1 << (index % 8));
}

int main(int argc, char *argv[]) {
    crc32_init();
    
    // 1. Parse command line arguments
    // We expect at least 7 arguments, but now can accept 9 for the optional seed
    if (argc != 7 && argc != 9) {
        fprintf(stderr, "Usage: %s --image <name> --size-kib <size> --inodes <count> [--seed <value>]\n", argv[0]);
        return 1;
    }
    
    char *image_path = NULL;
    uint64_t size_kib = 0;
    uint64_t inode_count = 0;
    
    for (int i = 1; i < argc; i += 2) {
        if (strcmp(argv[i], "--image") == 0) {
            image_path = argv[i+1];
        } else if (strcmp(argv[i], "--size-kib") == 0) {
            size_kib = atoll(argv[i+1]);
        } else if (strcmp(argv[i], "--inodes") == 0) {
            inode_count = atoll(argv[i+1]);
        // NEW: Add logic to parse the --seed argument
        } else if (strcmp(argv[i], "--seed") == 0) {
            g_random_seed = strtoull(argv[i+1], NULL, 10);
        } else {
            fprintf(stderr, "Error: Unknown argument '%s'\n", argv[i]);
            return 1;
        }
    }

    // NEW: If seed was not provided, use current time as a seed
    if (g_random_seed == 0) {
        g_random_seed = time(NULL);
    }
    
    if (!image_path || size_kib == 0 || inode_count == 0) {
        fprintf(stderr, "Error: Missing or invalid arguments.\n");
        return 1;
    }
    
    if (size_kib < 180 || size_kib > 4096 || (size_kib % 4 != 0)) {
        fprintf(stderr, "Error: Size must be a multiple of 4 between 180 and 4096 KiB.\n");
        return 1;
    }
    
    if (inode_count < 128 || inode_count > 512) {
        fprintf(stderr, "Error: Inode count must be between 128 and 512.\n");
        return 1;
    }
    
    // 2. Calculate file system layout
    uint64_t total_blocks = size_kib * 1024 / BS;
    uint32_t inodes_per_block = BS / INODE_SIZE;
    uint64_t inode_table_blocks = (inode_count + inodes_per_block - 1) / inodes_per_block;
    
    uint64_t inode_bitmap_start = 1;
    uint64_t data_bitmap_start = 2;
    uint64_t inode_table_start = 3;
    uint64_t data_region_start = inode_table_start + inode_table_blocks;
    uint64_t data_region_blocks = total_blocks - data_region_start;
    
    if (data_region_start >= total_blocks) {
        fprintf(stderr, "Error: File system too small for requested inode count.\n");
        return 1;
    }
    
    // 3. Allocate memory for the entire file system image
    uint64_t image_size = total_blocks * BS;
    uint8_t *fs_image = calloc(1, image_size);
    if (!fs_image) {
        perror("Failed to allocate memory for file system image");
        return 1;
    }
    
    // 4. Set up pointers to different regions
    superblock_t *sb = (superblock_t *)fs_image;
    uint8_t *inode_bitmap = fs_image + inode_bitmap_start * BS;
    uint8_t *data_bitmap = fs_image + data_bitmap_start * BS;
    inode_t *inode_table = (inode_t *)(fs_image + inode_table_start * BS);
    
    // 5. Initialize superblock
    time_t now = time(NULL);
    sb->magic = 0x4D565346;
    sb->version = 1;
    sb->block_size = BS;
    sb->total_blocks = total_blocks;
    sb->inode_count = inode_count;
    sb->inode_bitmap_start = inode_bitmap_start;
    sb->inode_bitmap_blocks = 1;
    sb->data_bitmap_start = data_bitmap_start;
    sb->data_bitmap_blocks = 1;
    sb->inode_table_start = inode_table_start;
    sb->inode_table_blocks = inode_table_blocks;
    sb->data_region_start = data_region_start;
    sb->data_region_blocks = data_region_blocks;
    sb->root_inode = ROOT_INO;
    sb->mtime_epoch = now;
    sb->flags = 0;
    
    // 6. Initialize root directory
    set_bit(inode_bitmap, ROOT_INO - 1);
    set_bit(data_bitmap, 0);
    
    inode_t *root_inode = &inode_table[ROOT_INO - 1];
    root_inode->mode = MODE_DIR;
    root_inode->links = 2;
    root_inode->uid = 0;
    root_inode->gid = 0;
    root_inode->size_bytes = BS;
    root_inode->atime = now;
    root_inode->mtime = now;
    root_inode->ctime = now;
    root_inode->direct[0] = data_region_start;
    
    // 7. Create root directory entries
    dirent64_t *root_dir = (dirent64_t *)(fs_image + data_region_start * BS);
    
    root_dir[0].inode_no = ROOT_INO;
    root_dir[0].type = TYPE_DIR;
    strcpy(root_dir[0].name, ".");
    
    root_dir[1].inode_no = ROOT_INO;
    root_dir[1].type = TYPE_DIR;
    strcpy(root_dir[1].name, "..");
    
    // 8. Finalize checksums (MUST BE DONE LAST)
    dirent_checksum_finalize(&root_dir[0]);
    dirent_checksum_finalize(&root_dir[1]);
    inode_crc_finalize(root_inode);
    superblock_crc_finalize(sb);
    
    // 9. Write to output file
    FILE *fp = fopen(image_path, "wb");
    if (!fp) {
        perror("Failed to open output image file");
        free(fs_image);
        return 1;
    }
    
    size_t written = fwrite(fs_image, 1, image_size, fp);
    if (written != image_size) {
        fprintf(stderr, "Error: Failed to write complete image file\n");
        fclose(fp);
        free(fs_image);
        return 1;
    }
    
    fclose(fp);
    free(fs_image);
    
    printf("Successfully created MiniVSFS image '%s'\n", image_path);
    printf("  Size: %" PRIu64 " KiB (%" PRIu64 " blocks)\n", size_kib, total_blocks);
    printf("  Inodes: %" PRIu64 "\n", inode_count);
    printf("  Data region: %" PRIu64 " blocks\n", data_region_blocks);
    // NEW: Print the seed that was used
    printf("  Seed: %" PRIu64 "\n", g_random_seed);
    
    return 0;
}
