#include "inode_layer.h"
#include <unistd.h>

/*
 * Part IA: inode allocation
 */

/* Allocate an inode and return its inum (inode number). */
uint32_t
inode_layer::alloc_inode(uint32_t type)
{
    /* 
     * Your Part I code goes here.
     * hint1: read get_inode() and put_inode(), read INODE_NUM, IBLOCK, etc in inode_layer.h
     * hint2: you can use time(0) to get the timestamp for atime/ctime/mtime
     */
    int inum = 0;
    struct inode *ino, *ino_disk;
    char buf[BLOCK_SIZE];
    for(int inode = 0; inode<INODE_NUM; inode++) {
        bm->read_block(IBLOCK(inode, bm->sb.nblocks), buf);
        ino_disk = (struct inode*)buf + inum%IPB;

        if (ino_disk->type == 0) {
            ino_disk->type = type;
            ino_disk->size = 0;
            ino_disk->atime = time(0);
            ino_disk->mtime = time(0);
            ino_disk->ctime = time(0);
            inum = inode;
            bm->write_block(IBLOCK(inum, bm->sb.nblocks), buf);
            break;
        }
    }
    return inum;
    /* Your Part I code ends here. */
}

/* Return an inode structure by inum, NULL otherwise.
 * Caller should release the memory. */
struct inode* 
inode_layer::get_inode(uint32_t inum)
{
    struct inode *ino, *ino_disk;
    char buf[BLOCK_SIZE];

    /* checking parameter */
    printf("\tim: get_inode %d\n", inum);
    if (inum < 0 || inum >= INODE_NUM) {
        printf("\tim: inum out of range\n");
        return NULL;
    }

    /* read the disk block containing the inode data structure */
    bm->read_block(IBLOCK(inum, bm->sb.nblocks), buf);
    ino_disk = (struct inode*)buf + inum%IPB;

    /* try to read an inode that is not in use */
    if (ino_disk->type == 0) {
        printf("\tim: inode not exist\n");
        return NULL;
    }

    /* return the inode data structure */
    ino = (struct inode*)malloc(sizeof(struct inode));
    *ino = *ino_disk;
    return ino;
}

void
inode_layer::put_inode(uint32_t inum, struct inode *ino)
{
    char buf[BLOCK_SIZE];
    struct inode *ino_disk;

    /* checking parameter */
    printf("\tim: put_inode %d\n", inum);
    if (inum < 0 || inum >= INODE_NUM || ino == NULL) {
        printf("\tim: invalid parameter\n");
        return;
    }

    /* modify the inode data structure on disk */
    bm->read_block(IBLOCK(inum, bm->sb.nblocks), buf);
    ino_disk = (struct inode*)buf + inum%IPB;
    *ino_disk = *ino;
    bm->write_block(IBLOCK(inum, bm->sb.nblocks), buf);
}


/*
 * Part IB: inode read/write
 */

blockid_t
block_layer::alloc_block()
{
    pthread_mutex_lock(&bitmap_mutex);
    /*
     * Your Part I code goes here.
     * hint1: go through the bitmap region, find a free data block, mark the corresponding bit in the bitmap to 1 and return the block number of the data block.
     * hint2: read free_block(); remember to call pthread_mutex_unlock before all the returns
     */
    char buf[BLOCK_SIZE];
    int start = (INODE_NUM/IPB) + 2;
    // int start = IBLOCK(INODE_NUM, sb.nblocks) + 1;
    int block_num = 0;

    for(int block= start; block<BLOCK_NUM; block++) {
        d->read_block(BBLOCK(start), buf);
        uint32_t bit_offset_in_block = block % BPB;
        uint32_t byte_offset_in_block = bit_offset_in_block / 8;
        uint32_t bit_offset_in_byte = bit_offset_in_block % 8;
        char* byte = &((char*)buf)[byte_offset_in_block];

        if ((*byte & ~(1 << bit_offset_in_byte)) == *byte) {
            *byte ^= (1 << bit_offset_in_byte);
            block_num = block;
            d->write_block(BBLOCK(block), buf);
            break;
        }
    }
    /* Your Part I code ends here. */
    pthread_mutex_unlock(&bitmap_mutex);
    /* no free data block left */
    return block_num;
}

void
block_layer::free_block(uint32_t id)
{
    pthread_mutex_lock(&bitmap_mutex);
    char buf[BLOCK_SIZE];
    d->read_block(BBLOCK(id), buf);

    /* suppose id=5001, we need to modify the 5001 th bit */
    /* which is the 5001 % 4096 = 905 th bit in the block*/
    uint32_t bit_offset_in_block = id % BPB;
    /* which lives in the 905 / 8 = 113 th byte in the block */
    uint32_t byte_offset_in_block = bit_offset_in_block / 8;
    /* and is the 905 % 8 = 1 st bit in this byte */
    uint32_t bit_offset_in_byte = bit_offset_in_block % 8;

    /* You may need to learn the meaning of &= and << operators */
    char* byte = &((char*)buf)[byte_offset_in_block];
    /* (char)1 is (00000001)binary */
    /* (char)1 << 1 is (00000010)binary */
    /* ~((char)1 << 1) is (11111101)binary */
    /* &= makes the bit representing id to zero */
    *byte &= ~((char)1 << bit_offset_in_byte);

    d->write_block(BBLOCK(id), buf);
    pthread_mutex_unlock(&bitmap_mutex);
}


#define MIN(a,b) ((a)<(b) ? (a) : (b))

/* Get all the data of a file by inum. 
 * Return alloced data, should be freed by caller. */
void
inode_layer::read_file(uint32_t inum, char **buf_out, int *size)
{
    rwlocks[inum].reader_enter();
    /* check parameter */
    if(buf_out == NULL || size == NULL) {
        rwlocks[inum].reader_exit();
        return;
    }

    /* check existance of inode inum */
    struct inode* ino = get_inode(inum);
    if(ino == NULL) {
        rwlocks[inum].reader_exit();
        return;
    }

    /* modify the access time of inode inum */
    ino->atime = time(NULL);
    put_inode(inum, ino);

    /* prepare the return value */
    *size = ino->size;
    char* buf = (char*)malloc(*size);

    /*
     * Your Part I code goes here.
     * hint1: read all blocks of inode inum into buf_out, including direct and indirect blocks; you will need the memcpy function
     */
    int blocks_to_read = (int)((*size+BLOCK_SIZE-1)/BLOCK_SIZE);
    char* total_buf = (char*)malloc(BLOCK_SIZE*blocks_to_read);
    int direct_blocks = blocks_to_read > NDIRECT ? NDIRECT : blocks_to_read;

    /*Loop through direct blocks */
    for(int i=0; i<direct_blocks; i++){
        bm->read_block(ino->blocks[i], total_buf + i*BLOCK_SIZE);
    }

    /*Loop through indirect block */
    if(blocks_to_read > NDIRECT) {
        char* indirect_buf = (char*)malloc(BLOCK_SIZE);
        bm->read_block(ino->blocks[NDIRECT], indirect_buf);
        for(int i=0; i<blocks_to_read-NDIRECT; i++) {
            char* block_arr = (char*)malloc(4);
            memcpy(block_arr, indirect_buf + i*4, 4);
            uint32_t block = (uint32_t)(block_arr[3] & ((1<<8)-1)) << 24 |
                    (uint32_t)(block_arr[2] & ((1<<8)-1)) << 16 |
                    (uint32_t)(block_arr[1] & ((1<<8)-1)) << 8  |
                    (uint32_t)(block_arr[0] & ((1<<8)-1));
            bm->read_block(block, total_buf + i*BLOCK_SIZE + (NDIRECT*BLOCK_SIZE));
        }
    }
    memcpy(buf, total_buf, *size);
    /* Your Part I code ends here. */
    *buf_out = buf;
    free(ino);
    rwlocks[inum].reader_exit();
}

void
inode_layer::write_file(uint32_t inum, const char *buf, int size)
{
    rwlocks[inum].writer_enter();
    /* check parameter */
    if(size < 0 || (uint32_t)size > BLOCK_SIZE * MAXFILE){
        printf("\tim: error! size negative or too large.\n");
        rwlocks[inum].writer_exit();
        return;
    }

    struct inode* ino = get_inode(inum);
    if(ino==NULL){
        printf("\tim: error! inode not exist.\n");
        rwlocks[inum].writer_exit();
        return;
    }


    /*
     * Your Part I code goes here.
     * hint1: buf is a buffer containing size blocks, write buf to blocks of inode inum.
     * you need to consider the situation when size
     * is larger or smaller than the size of inode inum
     */
    int blocks_to_write = (int)((size+BLOCK_SIZE-1)/BLOCK_SIZE);
    int last_bytes = size%BLOCK_SIZE;
    if (last_bytes == 0) {
        last_bytes = BLOCK_SIZE;
    }
    int direct_blocks = blocks_to_write > NDIRECT ? NDIRECT : blocks_to_write;
    int current_blocks = (int)((ino->size+BLOCK_SIZE-1)/BLOCK_SIZE);
    int current_direct_blocks = current_blocks > NDIRECT ? NDIRECT : current_blocks;

    /*Write to direct blocks*/
    for(int i=0; i<direct_blocks; i++){
        blockid_t block;
        if(i>current_blocks-1) {
            block = bm->alloc_block();
        }
        else {
            block = ino->blocks[i];
        }
        char* block_buf = (char*)malloc(BLOCK_SIZE);
        if(i == direct_blocks-1) {
            memcpy(block_buf, buf + (i*BLOCK_SIZE), last_bytes);
        }
        else {
            memcpy(block_buf, buf + (i*BLOCK_SIZE), BLOCK_SIZE);
        }
        bm->write_block(block, block_buf);
        ino->blocks[i]=block;
    }

    /*Free unused direct blocks */
    for(int i=0; i<current_direct_blocks - direct_blocks; i++){
        bm->free_block(ino->blocks[i+direct_blocks]);
    }
    
    /*Free indirect block*/
    if(current_blocks > NDIRECT) {
        int blocks_to_free = (int)((ino->size+BLOCK_SIZE-1)/BLOCK_SIZE);
        char* indirect_buf = (char*)malloc(BLOCK_SIZE);
        bm->read_block(ino->blocks[NDIRECT], indirect_buf);
        for(int i=0; i<blocks_to_free-NDIRECT; i++) {
            char* block_arr = (char*)malloc(4);
            memcpy(block_arr, indirect_buf + i*4, 4);
            uint32_t block = (uint32_t)(block_arr[3] & ((1<<8)-1)) << 24 |
                    (uint32_t)(block_arr[2] & ((1<<8)-1)) << 16 |
                    (uint32_t)(block_arr[1] & ((1<<8)-1)) << 8  |
                    (uint32_t)(block_arr[0] & ((1<<8)-1));
            bm->free_block(block);
        }
        bm->free_block(ino->blocks[NDIRECT]);
    }

    /*Write to indirect blocks*/
    if(blocks_to_write > NDIRECT) {
        blockid_t indirect_block = bm->alloc_block();
        int extra_needed = blocks_to_write - NDIRECT;
        char* indirect_buf = (char*)malloc(4*extra_needed);

        for(int i=0; i<extra_needed; i++) {
            blockid_t block = bm->alloc_block();
            char* block_buf = (char*)malloc(BLOCK_SIZE);
            if(i == extra_needed-1) {
                memcpy(block_buf, buf + (i*BLOCK_SIZE) + (NDIRECT*BLOCK_SIZE), last_bytes);
            }
            else {
                memcpy(block_buf, buf + (i*BLOCK_SIZE) + (NDIRECT*BLOCK_SIZE), BLOCK_SIZE);
            }
            bm->write_block(block, block_buf);
            memcpy(indirect_buf + i*4, &block , 4);
        }
        bm->write_block(indirect_block, indirect_buf);
        ino->blocks[NDIRECT]=indirect_block; 
    }
    ino->atime = time(0);
    ino->mtime = time(0);
    ino->ctime = time(0);
    ino->size=size;
    /* Your Part I code ends here. */
    put_inode(inum, ino);
    free(ino);
    rwlocks[inum].writer_exit(); 
}


/*
 * Part IC: inode free/remove
 */

void
inode_layer::free_inode(uint32_t inum)
{
    /* 
     * Your Part I code goes here.
     * hint1: simply mark inode inum as free
     */
    struct inode* ino = get_inode(inum);
    ino->type=0;
    put_inode(inum, ino);
    free(ino);
    /* Your Part I code ends here. */
}



void
inode_layer::remove_file(uint32_t inum)
{
    struct inode* ino = get_inode(inum);
    if(ino == NULL)
        return;

    /*
     * Your Part I code goes here.
     * hint1: first, free all data blocks of inode inum (use bm->free_block function); second, free the inode
     */
    int blocks_to_free = (int)((ino->size+BLOCK_SIZE-1)/BLOCK_SIZE);
    int direct_blocks = blocks_to_free > NDIRECT ? NDIRECT : blocks_to_free;

    /*Loop through direct blocks */
    for(int i=0; i<direct_blocks; i++){
        bm->free_block(ino->blocks[i]);
    }

    /*Loop through indirect block */
    if(blocks_to_free > NDIRECT) {
        char* indirect_buf = (char*)malloc(BLOCK_SIZE);
        bm->read_block(ino->blocks[NDIRECT], indirect_buf);
        for(int i=0; i<blocks_to_free-NDIRECT; i++) {
            char* block_arr = (char*)malloc(4);
            memcpy(block_arr, indirect_buf + i*4, 4);
            uint32_t block = (uint32_t)(block_arr[3] & ((1<<8)-1)) << 24 |
                    (uint32_t)(block_arr[2] & ((1<<8)-1)) << 16 |
                    (uint32_t)(block_arr[1] & ((1<<8)-1)) << 8  |
                    (uint32_t)(block_arr[0] & ((1<<8)-1));
            bm->free_block(block);
        }
        bm->free_block(ino->blocks[NDIRECT]);
    }
    free_inode(inum);
    /* Your Part I code ends here. */
    free(ino);
    return;
}



/*
 * Helper Functions
 */

/* inode layer ---------------------------------------- */

inode_layer::inode_layer()
{
    bm = new block_layer();
    uint32_t root_dir = alloc_inode(fs_protocol::T_DIR);
    if (root_dir != 0) {
        printf("\tim: error! alloc first inode %d, should be 0\n", root_dir);
        exit(0);
    }
}

void
inode_layer::getattr(uint32_t inum, fs_protocol::attr &a)
{
    struct inode* ino = get_inode(inum);
    if(ino == NULL)
        return;
    a.type = ino->type;
    a.size = ino->size;
    a.atime = ino->atime;
    a.mtime = ino->mtime;
    a.ctime = ino->ctime;
    free(ino);
}


/* block layer ---------------------------------------- */

// The layout of disk should be like this:
// |<-sb->|<-free block bitmap->|<-inode table->|<-data->|
block_layer::block_layer()
{
    d = new disk();

    // format the disk
    sb.size = BLOCK_SIZE * BLOCK_NUM;
    sb.nblocks = BLOCK_NUM;
    sb.ninodes = INODE_NUM;

}

void
block_layer::read_block(uint32_t id, char *buf)
{
    d->read_block(id, buf);
}

void
block_layer::write_block(uint32_t id, const char *buf)
{
    d->write_block(id, buf);
}


/* disk layer ---------------------------------------- */

disk::disk()
{
  bzero(blocks, sizeof(blocks));
}

void
disk::read_block(blockid_t id, char *buf)
{
    if(id < 0 || id > BLOCK_NUM || buf == NULL)
        return;
    memcpy(buf, blocks[id], BLOCK_SIZE);

    if (DISK_ACCESS_LATENCY)
        usleep(DISK_ACCESS_LATENCY);
}

void
disk::write_block(blockid_t id, const char *buf)
{
    if(id < 0 || id > BLOCK_NUM || buf == NULL)
        return;
    memcpy(blocks[id], buf, BLOCK_SIZE);

    if (DISK_ACCESS_LATENCY)
        usleep(DISK_ACCESS_LATENCY);
}
