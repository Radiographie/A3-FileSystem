#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include "ext2.h"

#define BLOCK_NUM   128
#define BLOCK_SIZE  1024

unsigned char *ptr_disk;
int get_path_table(char *table_path[10], char *ptr_path);
int dir_exist(struct ext2_dir_entry_2 *dir_ptr, char *dir);
int get_free_block(unsigned char *bm_block, struct ext2_super_block *ptr_sb);
int get_free_inode(unsigned char *bm_inode, struct ext2_super_block *ptr_sb);
struct ext2_dir_entry_2 * recurse_inode(struct ext2_inode *inode_ptr, char *path_array[10], int path_array_len, int index, int bg_inode_table);


#define SETBIT(t, pos) (*((t) + ((pos) / 8)) = (*((t) + ((pos) / 8)) & ~(1 << ((pos) % 8))) | (1 << ((pos) % 8)));

int get_path_table(char *table_path[10], char *ptr_path)
{
    char *work = strtok(ptr_path, "/");
    int count = 0;
    
    while(work != NULL)
    {
        table_path[count] = work;
        count++;
        work = strtok(NULL, "/");
    }
    return count;
}

int dir_exist(struct ext2_dir_entry_2 *dir_ptr, char *dir)
{
    int dir_entry_size = 0;
    
    for (dir_entry_size = 0; dir_entry_size < EXT2_BLOCK_SIZE;)
    {
        if (dir_ptr->file_type == EXT2_FT_REG_FILE)
        {
            char *name = malloc(sizeof(char *));
            int len_name = dir_ptr->name_len;
            strncpy(name, dir_ptr -> name, len_name);
            if(strcmp(name, dir) == 0)
            {
                return dir_ptr -> inode;
            }
        }
        if (dir_ptr->inode != 2)
        {
            if (dir_ptr->inode != 11)
                {
                    if (strncmp(dir_ptr->name, "..", 2) != 0)
                        {
                            if (strncmp(dir_ptr->name, ".", 1) != 0)
                            {
                                char *dir_name = malloc(sizeof(char *));
                                int len1 = dir_ptr -> name_len;
                                strncpy(dir_name, dir_ptr -> name, len1);
                            if(strcmp(dir_name, dir) == 0)
                            {
                                return dir_ptr -> inode;
                            }
                            }
                        }
                }
            
        }
        dir_entry_size += dir_ptr->rec_len;
        dir_ptr = (void *)dir_ptr + dir_ptr->rec_len;
    }
    
    return 0;
}

int get_free_block(unsigned char *bm_block, struct ext2_super_block *ptr_sb)
{
    int i;
    for(i = 0; i < (ptr_sb->s_blocks_count / 8); i++)
    {
        char c = *bm_block;
        int j;
        for (j = 0; j < 8; j++)
        {
            if(!(c & (1 << j)))
            {
                return i * 8 + j;
            }
        }
        bm_block =  bm_block + sizeof(char);
    }
    return -1;
}

int get_free_inode(unsigned char *bm_inode, struct ext2_super_block *ptr_sb)
{
    int i;
    for (i = 0; i < (ptr_sb->s_inodes_count / 8); i++)
    {
        char c = *bm_inode;
        int j;
        for (j = 0; j < 8; j++)
        {
            if(!(c & (1 << j)))
            {
                return i * 8 + j;
            }
        }
        bm_inode =  bm_inode + sizeof(char);
    }
    return -1;
}

struct ext2_dir_entry_2 * get_free_space(struct ext2_inode *current_inode, int req_len)
{
    struct ext2_dir_entry_2 *ptr_dir;
    int i = 0;
    int data_block_num;
    int found = 0;
    
    while (i < 12)
    {
        data_block_num = current_inode -> i_block[i];
        ptr_dir = (void *)ptr_disk + EXT2_BLOCK_SIZE * data_block_num;
        
        int size;
        for (size = 0; size < EXT2_BLOCK_SIZE;)
        {
            size += ptr_dir -> rec_len;
            if(size == EXT2_BLOCK_SIZE &&
               ptr_dir -> rec_len >= (4 * (ptr_dir -> name_len) + req_len + 8))
            {
                return ptr_dir;
                break;
            }
            ptr_dir = (void *)ptr_dir + ptr_dir->rec_len;
        }
        if(found == 1)
        {
            break;
        }
        i++;
    }
    return 0;
}

struct ext2_dir_entry_2 * recurse_inode(struct ext2_inode *inode_ptr, char *path_array[10],
                                        int path_array_len, int index, int bg_inode_table)
{
    int block_id;   // to record the number of used block
    if(path_array_len == 1)
    {   // case: path is the root path
        int i = 0;
        while(i < 11)
        {
            if (inode_ptr->i_block[i] > 0)
            {   // the block is in use
                block_id = inode_ptr->i_block[i];
                struct ext2_dir_entry_2 *ptr_dir = (struct ext2_dir_entry_2 *)(ptr_disk + EXT2_BLOCK_SIZE * block_id);
                return ptr_dir;
            }
            i++;
        }
    }
    
    
    int i  = 0;
    while (i < 12)
    {
        if (inode_ptr->i_block[i] > 0)
        {
            block_id = inode_ptr->i_block[i];
            struct ext2_dir_entry_2 *ptr_dir = (struct ext2_dir_entry_2 *)(ptr_disk + EXT2_BLOCK_SIZE * block_id);

            // check if the dir exists and find its inode number
            int dir_inode_ptr = dir_exist(ptr_dir, path_array[index]);
            if(dir_inode_ptr != 0)
            {   // case: the dir exists
                if(path_array_len - index == 2)
                {
                    struct ext2_inode *inode_ptr2 = (struct ext2_inode *)(ptr_disk + EXT2_BLOCK_SIZE * bg_inode_table);
                    inode_ptr2 += dir_inode_ptr-1;
                    block_id = inode_ptr2->i_block[i ];
                    ptr_dir = (struct ext2_dir_entry_2 *)(ptr_disk + EXT2_BLOCK_SIZE * block_id);
                    return ptr_dir;
                } else if (path_array_len - index == 1){
                    return 0;
                } else {
                    struct ext2_inode *inode_ptr2 = (struct ext2_inode *)(ptr_disk + EXT2_BLOCK_SIZE * bg_inode_table);
                    inode_ptr2 += dir_inode_ptr-1;
                    return recurse_inode(inode_ptr2, path_array, path_array_len, index+1, bg_inode_table);
                }
            }
        }
        i ++;
    }
    return 0;
}



int main(int argc, char **argv) {

    if (argc != 4)
    {
        fprintf(stderr, "Usage: readimg <image file name> filepath \n");
        exit(1);
    }
    
    // open source file
    int fd_src = open(argv[2], O_RDONLY);
    if (fd_src < 0){
        fprintf(stderr, "No such file exists in the native file system \n");
        exit(ENOENT);
    }

    int filesize_src = lseek(fd_src, 0, SEEK_END);
    // mmap cannot map size 0 file.
    if (filesize_src <= 0){
        fprintf(stderr, "The source file is empty \n");
        exit(ENOENT);
    }

    int req_block_num = (filesize_src - 1) / EXT2_BLOCK_SIZE + 1;

    // open the image
    int fd = open(argv[1], O_RDWR);
    if (fd < 0){
        fprintf(stderr, "No such virtual disk exists \n");
        exit(ENOENT);   
    }

    // map the virtual disk and source file on memory
    ptr_disk = mmap(NULL, BLOCK_NUM * BLOCK_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    unsigned char* ptr_disk_src = mmap(NULL, filesize_src, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd_src, 0);
    if (ptr_disk == MAP_FAILED || ptr_disk_src == MAP_FAILED)
    {
       perror("mmap");
       exit(1);
    }

    struct ext2_group_desc *ptr_group_desc = (struct ext2_group_desc *)(ptr_disk + EXT2_SB_OFFSET + EXT2_BLOCK_SIZE);
    // get the inode table
    struct ext2_inode *ptr_inode = (struct ext2_inode *)(ptr_disk + EXT2_BLOCK_SIZE * ptr_group_desc->bg_inode_table);
    unsigned char *bm_inode = (ptr_disk + (ptr_group_desc -> bg_inode_bitmap) * BLOCK_SIZE);
    unsigned char *bm_block = (ptr_disk + (ptr_group_desc -> bg_block_bitmap) * BLOCK_SIZE);
    struct ext2_super_block *ptr_super_block = (struct ext2_super_block *)(ptr_disk + EXT2_SB_OFFSET);
    char *ptr_path_src = strdup(argv[2]);
    char *ptr_path_dest = strdup(argv[3]);
    char *table_path_src[10];
    char *table_path_dest[10];
    int count_path_src = get_path_table(table_path_src, ptr_path_src);
    int count_path_dest = get_path_table(table_path_dest, ptr_path_dest);

    // Find the destination directory in the virtual disk
    struct ext2_dir_entry_2 *ptr_dir_dest = recurse_inode(ptr_inode + 1, table_path_dest, count_path_dest, 0, ptr_group_desc->bg_inode_table);

    if(ptr_dir_dest == 0)
    {
        printf("No such directory exists on virtual disk\n");
        return -1;
    }

    int table_new_block[req_block_num];
    int copy_flag = 0;

    int i = 0;
    while(i < req_block_num)
    {
        int free_block = get_free_block(bm_block, ptr_super_block);
        SETBIT(bm_block, free_block);
        table_new_block[i] = free_block;

        if(filesize_src - copy_flag <= BLOCK_SIZE)
        {
            memcpy(ptr_disk + EXT2_BLOCK_SIZE * (free_block + 1), ptr_disk_src + BLOCK_SIZE * i, filesize_src - copy_flag - 1);
        }
        else
        {
            memcpy(ptr_disk + EXT2_BLOCK_SIZE * (free_block + 1), ptr_disk_src + BLOCK_SIZE * i, EXT2_BLOCK_SIZE);
            copy_flag += BLOCK_SIZE;
        }
        i++;
    }

    int block_id_inderect;
    if(req_block_num > 11)
    {
        block_id_inderect = get_free_block(bm_block, ptr_super_block);
        int *indirect_block = (int *)(ptr_disk + EXT2_BLOCK_SIZE * (block_id_inderect + 1));
        SETBIT(bm_block, block_id_inderect);
        for(i = 12; i < req_block_num; i++)
        {
            *indirect_block = table_new_block[i];
            indirect_block++;
        }
    }

    int fr_inode = get_free_inode(bm_inode, ptr_super_block);
    struct ext2_inode *node_new = ptr_inode + (fr_inode);
    SETBIT(bm_inode, fr_inode);
    node_new->i_links_count = 1;
    node_new->i_mode = EXT2_S_IFREG;
    node_new->i_size = filesize_src;
    node_new->i_blocks = req_block_num * 2;

    i = 0;
    while (i < 11 && req_block_num != i)
    {
        node_new->i_block[i] = table_new_block[i];
        i++;
    }

    if(req_block_num > 11)
    {
        node_new -> i_block[12] = block_id_inderect;
    }

    struct ext2_inode *in_src = (struct ext2_inode *)(ptr_disk + EXT2_BLOCK_SIZE * ptr_group_desc->bg_inode_table);
    struct ext2_inode *in_cur = (struct ext2_inode *)(ptr_disk + EXT2_BLOCK_SIZE * ptr_group_desc->bg_inode_table);
    in_src = in_src + ptr_dir_dest -> inode - 1;
    in_cur = in_cur + ptr_dir_dest -> inode - 1;
    in_cur->i_links_count++;

    int len_req = strlen(table_path_src[count_path_src - 1]) + 8;
    struct ext2_dir_entry_2 *ptr_dir = get_free_space(in_cur, len_req);

    if(ptr_dir == 0)
    {
        int free_block_new = get_free_block(bm_block, ptr_super_block);
        ptr_dir = (void*)ptr_disk + EXT2_BLOCK_SIZE * (free_block_new + 1);
        ptr_dir -> inode = fr_inode + 1;
        ptr_dir -> name_len = strlen(table_path_dest[count_path_dest - 1]);
        ptr_dir -> file_type = EXT2_FT_DIR;
        strcpy(ptr_dir -> name, table_path_dest[count_path_dest - 1]);
        ptr_dir -> rec_len = BLOCK_SIZE;
        
        in_src -> i_blocks += 2;
        in_src ->i_block[in_src -> i_size / BLOCK_SIZE] = free_block_new + 1;
        in_src -> i_size += BLOCK_SIZE;
    }
    else
    {
        int new_rec_len = ptr_dir -> rec_len - (4 * (ptr_dir -> name_len) + 8);
        ptr_dir -> rec_len = 4 * (ptr_dir -> name_len) + 8;
        ptr_dir = (void*)ptr_dir + ptr_dir -> rec_len;
        ptr_dir -> rec_len = new_rec_len;
        ptr_dir -> inode = fr_inode + 1;
        ptr_dir -> name_len = strlen(table_path_dest[count_path_dest - 1]);
        ptr_dir -> file_type = EXT2_FT_REG_FILE;
        strcpy(ptr_dir -> name, table_path_dest[count_path_dest - 1]);
    }
    ptr_group_desc->bg_used_dirs_count++;
    return 0;
}