#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>
#include "ext2.h"


#define BLOCK_NUM   128
#define BLOCK_SIZE  1024

unsigned char *ptr_disk;
int get_path_table(char *table_path[10], char *ptr_path);
struct ext2_dir_entry_2 * recurse_inode(struct ext2_inode *inode_ptr, char *path_array[10],
                                        int path_array_len, int index, int bg_inode_table);
int dir_exist(struct ext2_dir_entry_2 *dir_ptr, char *dir);
int get_free_block(unsigned char *bm_block, struct ext2_super_block *ptr_sb);
struct ext2_dir_entry_2 * get_free_space(struct ext2_inode *current_inode, int req_len);

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
        if (dir_ptr->inode != 2)
        {
        	  if (dir_ptr->inode != 11)
        	  	{
        	  		 if (strncmp(dir_ptr->name, "..", 2) != 0)
        	  		 	{
        	  		 		 if (strncmp(dir_ptr->name, ".", 1) != 0)
        	  		 		 	{
        	  		 		 		char *dir_name = malloc(sizeof(char *));
						            strncpy(dir_name, dir_ptr -> name, dir_ptr -> name_len);
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

int main(int argc, char **argv) {

    if (argc != 4)
    {
        fprintf(stderr, "Usage: readimg <image file name> filepath \n");
        exit(1);
    }
    int fd = open(argv[1], O_RDWR);

    ptr_disk = mmap(NULL, BLOCK_NUM * BLOCK_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (ptr_disk == MAP_FAILED)
    {
       perror("mmap");
       exit(1);
    }
    struct ext2_group_desc *ptr_group_desc = (struct ext2_group_desc *)(ptr_disk + EXT2_SB_OFFSET + EXT2_BLOCK_SIZE);
    struct ext2_inode *ptr_in = (struct ext2_inode *)(ptr_disk + EXT2_BLOCK_SIZE * ptr_group_desc->bg_inode_table);


    char *ptr_path_src = strdup(argv[2]);
    char *ptr_path_dest = strdup(argv[3]);
    char *table_path_src[10];
    char *table_path_dest[10];
    int count_path_src = get_path_table(table_path_src, ptr_path_src);
    int count_path_dest = get_path_table(table_path_dest, ptr_path_dest);
    struct ext2_dir_entry_2 *ptr_dir_src = recurse_inode(ptr_in + 1, table_path_src, count_path_src, 0, ptr_group_desc->bg_inode_table);
    struct ext2_dir_entry_2 *ptr_dir_dest = recurse_inode(ptr_in + 1, table_path_dest, count_path_dest, 0, ptr_group_desc->bg_inode_table);

    if((ptr_dir_src == 0) || (ptr_dir_dest == 0))
    {
        printf("No such directory exists\n");
        return -1;
    }

    if(dir_exist(ptr_dir_dest, table_path_dest[count_path_dest - 1]) != 0)
    {
        printf("Directory already exists\n");
        return -1;
    }

    int in_file_src = dir_exist(ptr_dir_src, table_path_src[count_path_src - 1]);

    struct ext2_inode *in_src = (struct ext2_inode *)(ptr_disk + EXT2_BLOCK_SIZE * ptr_group_desc->bg_inode_table);
    in_src = in_src + ptr_dir_src -> inode - 1;

    struct ext2_inode *dest_inode = (struct ext2_inode *)(ptr_disk + EXT2_BLOCK_SIZE * ptr_group_desc->bg_inode_table);
    dest_inode = dest_inode + ptr_dir_dest -> inode - 1;
    dest_inode->i_links_count++;

    int req_len = strlen(table_path_dest[count_path_dest - 1]) + 8;
    struct ext2_dir_entry_2 *ptr_dir = get_free_space(dest_inode, req_len);

    if(ptr_dir != NULL)
    {
        int new_rec_len = ptr_dir -> rec_len - (4 * (ptr_dir -> name_len) + 8);
        ptr_dir -> rec_len = 4 * (ptr_dir -> name_len) + 8;
        ptr_dir = (void*)ptr_dir + ptr_dir -> rec_len;
        ptr_dir -> rec_len = new_rec_len;
        ptr_dir -> inode = in_file_src;
        ptr_dir -> name_len = strlen(table_path_dest[count_path_dest - 1]);
        ptr_dir -> file_type = EXT2_FT_REG_FILE;
        strcpy(ptr_dir -> name, table_path_dest[count_path_dest - 1]);
    }

    ptr_group_desc->bg_used_dirs_count++;
    return 0;
}

