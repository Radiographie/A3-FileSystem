#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "ext2.h"
#include <string.h>
#include <errno.h>

#define BLOCK_NUM   128
#define BLOCK_SIZE  1024
#define SETBIT(t, pos) (*((t) + ((pos) / 8)) = (*((t) + ((pos) / 8)) & ~(1 << ((pos) % 8))) | (1 << ((pos) % 8)));

unsigned char *ptr_disk;

int dir_exist(struct ext2_dir_entry_2 *dir_ptr, char *dir);

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

struct ext2_dir_entry_2 * recurse_inode(struct ext2_inode *inode_ptr, char *path_array[10],
                                        int path_array_len, int index, int bg_inode_table)
{
    int block_id;
    if(path_array_len == 1)
    {
        int i = 0;
        while(i < 11)
        {
            if (inode_ptr->i_block[i] > 0)
            {
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
        if (inode_ptr->i_block[i ] > 0)
        {
            block_id = inode_ptr->i_block[i];
            struct ext2_dir_entry_2 *ptr_dir = (struct ext2_dir_entry_2 *)(ptr_disk + EXT2_BLOCK_SIZE * block_id);
            int dir_inode_ptr = dir_exist(ptr_dir, path_array[index]);
            if(dir_inode_ptr != 0)
            {
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

int dir_exist(struct ext2_dir_entry_2 *dir_ptr, char *dir)
{
    int dir_entry_size = 0;
    
    for (dir_entry_size = 0; dir_entry_size < EXT2_BLOCK_SIZE;)
    {
        if (dir_ptr->file_type == EXT2_FT_REG_FILE)
        {
            char *name = malloc(sizeof(char *));
            strncpy(name, dir_ptr -> name, dir_ptr -> name_len);
            if(strcmp(name, dir) == 0)
            {
                return dir_ptr -> inode;
            }
        }
        if (dir_ptr->inode != 2 && dir_ptr->inode != 11
            && (strncmp(dir_ptr->name, "..", 2) != 0)
            && (strncmp(dir_ptr->name, ".", 1) != 0))
        {
            char *dir_name = malloc(sizeof(char *));
            strncpy(dir_name, dir_ptr -> name, dir_ptr -> name_len);
            if(strcmp(dir_name, dir) == 0)
            {
                return dir_ptr -> inode;
            }
        }
        dir_entry_size += dir_ptr->rec_len;
        dir_ptr = (void *)dir_ptr + dir_ptr->rec_len;
    }
    
    return 0;
}

void create_inode(struct ext2_inode *in_new, int fr_bk)
{
    in_new->i_mode = EXT2_S_IFDIR;
    in_new->i_size = BLOCK_SIZE;
    in_new->i_links_count = 2;
    in_new->i_blocks = 2;
    in_new->i_block[0] = fr_bk + 1;
}


void create_directory(struct ext2_dir_entry_2 *dir_new, int fr_in, int par_in)
{
    dir_new -> inode = fr_in + 1;
    dir_new -> name_len = 2;
    dir_new -> file_type = EXT2_FT_DIR;
    dir_new -> rec_len = 12;
    dir_new -> name[0] = '.';
    dir_new = (void *) dir_new + 12;
    
    dir_new -> inode = par_in;
    dir_new -> name_len = 3;
    dir_new -> rec_len = 1012;
    dir_new -> file_type = EXT2_FT_DIR;
    strcpy(dir_new -> name, "..");
}


int main(int argc, char **argv)
{

    if (argc != 3) {
        fprintf(stderr, "Usage: readimg <image file name> filepath \n");
        exit(1);
    }
    int fd = open(argv[1], O_RDWR);

    ptr_disk = mmap(NULL, 128 * BLOCK_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (ptr_disk == MAP_FAILED) {
	   perror("mmap");
	   exit(1);
    }
    struct ext2_group_desc *ptr_group_desc = (struct ext2_group_desc *)(ptr_disk + EXT2_SB_OFFSET + EXT2_BLOCK_SIZE);
    struct ext2_inode *ptr_in = (struct ext2_inode *)(ptr_disk + EXT2_BLOCK_SIZE * ptr_group_desc->bg_inode_table);
    unsigned char *bm_block = (ptr_disk + (ptr_group_desc -> bg_block_bitmap) * BLOCK_SIZE);
    unsigned char *bm_in = (ptr_disk + (ptr_group_desc -> bg_inode_bitmap) * BLOCK_SIZE);
    struct ext2_super_block *ptr_super_block = (struct ext2_super_block *)(ptr_disk + EXT2_SB_OFFSET);

    char *ptr_path_src = strdup(argv[2]);
    char *table_path_src[10];
    int path_count = get_path_table(table_path_src, ptr_path_src);

    struct ext2_dir_entry_2 *current_dir = recurse_inode(ptr_in + 1, table_path_src, path_count, 0, ptr_group_desc->bg_inode_table);
    if(current_dir == 0){
        printf("No such directory exists\n");
        exit(ENOENT);
    }

    if(dir_exist(current_dir, table_path_src[path_count - 1]) != 0)
    {
        printf("Directory already exists\n");
        exit(EEXIST);
    }

    int fr_in = get_free_inode(bm_in, ptr_super_block);
    int fr_bk = get_free_block(bm_block, ptr_super_block);

    if((fr_bk == -1) || (fr_in == -1)){
        printf("Not enough memory");
       exit(ENOENT);
    }

    struct ext2_dir_entry_2 *new_dir = ptr_disk + EXT2_BLOCK_SIZE * (fr_bk + 1);
    create_directory(new_dir, fr_in , current_dir -> inode);
    SETBIT(bm_block, fr_bk);

    struct ext2_inode *new_inode = ptr_in + (fr_in);
    create_inode(new_inode, fr_bk);
    SETBIT(bm_in, fr_in);

    struct ext2_inode *current_inode = (struct ext2_inode *)(ptr_disk + EXT2_BLOCK_SIZE * ptr_group_desc->bg_inode_table);
    current_inode = current_inode + current_dir -> inode - 1;
    current_inode->i_links_count++;

    int req_len = strlen(table_path_src[path_count - 1]) + 8;
    struct ext2_dir_entry_2 *dir_ptr = get_free_space(current_inode, req_len);

    if(dir_ptr != 0){
        int new_rec_len = dir_ptr -> rec_len - (4 * (dir_ptr -> name_len) + 8);
        dir_ptr -> rec_len = 4 * (dir_ptr -> name_len) + 8;
        dir_ptr = (void*)dir_ptr + dir_ptr -> rec_len;
        dir_ptr -> rec_len = new_rec_len;
        dir_ptr -> inode = fr_in + 1;
        dir_ptr -> name_len = strlen(table_path_src[path_count - 1]);
        dir_ptr -> file_type = EXT2_FT_DIR;
        strcpy(dir_ptr -> name, table_path_src[path_count - 1]);
    }else {
        int free_block_new = get_free_block(bm_block, ptr_super_block);
        dir_ptr = (void*)ptr_disk + EXT2_BLOCK_SIZE * (free_block_new + 1);        
        dir_ptr -> inode = fr_in + 1;
        dir_ptr -> name_len = strlen(table_path_src[path_count - 1]);
        dir_ptr -> file_type = EXT2_FT_DIR;
        strcpy(dir_ptr -> name, table_path_src[path_count - 1]);  
        dir_ptr -> rec_len = BLOCK_SIZE;

        current_inode -> i_blocks += 2; 
        current_inode ->i_block[current_inode -> i_size / BLOCK_SIZE] = free_block_new + 1;
        current_inode -> i_size += BLOCK_SIZE;
    }

    ptr_group_desc->bg_used_dirs_count++;
    return 0;
}