#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>
#include <errno.h>
#include "ext2.h"

#define BLOCK_NUM   128
#define BLOCK_SIZE  1024
#define RESETBIT(id, t) (*((t) + ((id) / 8)) = (*((t) + ((id) / 8)) & ~(1 << ((id) % 8))) | (0 << ((id) % 8)))

unsigned char *ptr_disk;
void scan_files(char *file, char *path, struct ext2_inode *inode_ptr, int bg_inode_table);
void recurse_files(char *path, char *file, struct ext2_dir_entry_2 *dir_ptr, int bg_inode_table);


void recurse_files(char *path, char *file, struct ext2_dir_entry_2 *ptr_dir, int bg_inode_table)
{
    int dir_entry_size = 0;
    int recurse_success = -1;
    char *end_flag = path;
    const char delimit[2] = "/";
    
    int dir_entry_size_p = 0;
    struct ext2_dir_entry_2 *dir_ptr_reset = ptr_dir;
    
    for (dir_entry_size = 0; dir_entry_size < EXT2_BLOCK_SIZE;)
    {
        if (end_flag != NULL)
        {
            if (ptr_dir->file_type == EXT2_FT_DIR)
            {
                if (strncmp(ptr_dir->name, path, strlen(path)) == 0)
                {
                    if (recurse_success == -1)
                        recurse_success = 0;
                    recurse_success = ptr_dir->inode;
                }
            }
            
        }
        //if token is null, we are in absolute path print all files.
        else
        {
            if (strncmp(ptr_dir->name, file, strlen(file)) == 0)
            {
                int limit_size = ptr_dir->rec_len;
                ptr_dir = (void *)dir_ptr_reset + dir_entry_size_p;
                ptr_dir->rec_len = ptr_dir->rec_len + limit_size;
                struct ext2_super_block *ptr_sb = (struct ext2_super_block *)(ptr_disk + EXT2_SB_OFFSET);
                struct ext2_group_desc *ptr_group_desc = (struct ext2_group_desc *)(ptr_disk + EXT2_SB_OFFSET + EXT2_BLOCK_SIZE);
                unsigned char *ptr_in_bm = (unsigned char *)(ptr_disk + EXT2_BLOCK_SIZE * ptr_group_desc->bg_inode_bitmap);
                unsigned char *ptr_bg_bm = (unsigned char *)(ptr_disk + EXT2_BLOCK_SIZE * ptr_group_desc->bg_block_bitmap);
                struct ext2_inode *inode_ptr = (struct ext2_inode *)(ptr_disk + EXT2_BLOCK_SIZE * ptr_group_desc->bg_inode_table);
                inode_ptr++;
                
                RESETBIT(ptr_dir->inode, ptr_in_bm);
                int i = 1;
                
                //set block bitmap to 0
                while (i < (EXT2_BLOCK_SIZE / sizeof(struct ext2_inode) * 8))
                {
                    if ((inode_ptr->i_ctime < 1) || (inode_ptr->i_dtime > 0) ||
                        ((i < EXT2_GOOD_OLD_FIRST_INO) && (i > 1)) )
                    {
                        inode_ptr++;
                        i++;
                        continue;
                    }
                    if (i != ptr_dir->inode-1)
                    {
                        inode_ptr++;
                        i++;
                        continue;
                    }
                    else
                    {
                        RESETBIT(i, ptr_bg_bm);
                        int j = 0;
                        while (j<15)
                        {
                            if (inode_ptr->i_block[j] <= 0)
                            {
                                j++;
                                continue;
                            }
                            else
                            {
                                RESETBIT(j+1, ptr_bg_bm);
                                ptr_sb->s_free_blocks_count++; //update file block count
                            }
                            j++;
                        }
                    }
                    inode_ptr++;
                    i++;
                }
            }
        }
        dir_entry_size_p = dir_entry_size;
        dir_entry_size += ptr_dir->rec_len;
        ptr_dir = (void *)ptr_dir + ptr_dir->rec_len;
    }
    
    //return if no path exists
    if (recurse_success == -1 && end_flag != NULL)
    {
        printf("No such file or directory\n");
        exit(1);
    }
    if (end_flag == NULL)
        exit(0);
    
    end_flag = strtok(NULL, delimit);
    
    struct ext2_inode *inode_ptr2 = (struct ext2_inode *)(ptr_disk + EXT2_BLOCK_SIZE * bg_inode_table);
    inode_ptr2 += recurse_success - 1;
    scan_files(end_flag, file, inode_ptr2, bg_inode_table);
}


/*
 * read all the inode of an inode
 */
void scan_files(char *path, char *file, struct ext2_inode *inode_ptr, int bg_inode_table)
{
    int block_id, i;
    
    i = 0;
    while (i < 12)
    {
        // if the block is not used, skip it
        if (inode_ptr->i_block[i] == 0)
        {
            i++;
            continue;
        }
        else
        {
            block_id = inode_ptr->i_block[i];
            struct ext2_dir_entry_2 *ptr_dir = (struct ext2_dir_entry_2 *)(ptr_disk + EXT2_BLOCK_SIZE * block_id);
            recurse_files(path, file, ptr_dir, bg_inode_table);
        }
        i++;
    }
}

int main(int argc, char *argv[])
{

    if (argc != 3)
    {
        fprintf(stderr, "Usage: ext2_rm <image file name> <file path name>\n");
        exit(1);
    }
    
    int fd = open(argv[1], O_RDWR);

    ptr_disk = mmap(NULL, 128 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    
    if (ptr_disk == MAP_FAILED)
    {
	   perror("mmap");
	   exit(1);
    }

    char *ptr_filepath = argv[2];
    if (ptr_filepath[strlen(ptr_filepath) - 1] == '/')
    {
        printf("%s is a directory!\n", ptr_filepath);
        exit(EISDIR);
    }

    char filename[256];
    int start_pos = strlen(ptr_filepath) - 1;
    
    while (ptr_filepath[start_pos] != '/')
    {
        start_pos--;
    }
    strcpy(filename, ptr_filepath + start_pos + 1);
    if (start_pos == 0)
        ptr_filepath[start_pos + 1] = '\0';
    else
        ptr_filepath[start_pos] = '\0';
    printf("path : %s filename : %s\n", ptr_filepath, filename);

    //start of blockgroupdes
    struct ext2_group_desc *ptr_group_desc = (struct ext2_group_desc *)(ptr_disk + EXT2_SB_OFFSET + EXT2_BLOCK_SIZE);
    //point to first inode
    struct ext2_inode *inode_ptr = (struct ext2_inode *)(ptr_disk + EXT2_BLOCK_SIZE * ptr_group_desc->bg_inode_table);
    //points to root inode
    inode_ptr++;

    const char delimit[2] = "/";
    char *token = strtok(ptr_filepath, delimit);
    scan_files(token, filename, inode_ptr, ptr_group_desc->bg_inode_table);

    return 0;
}