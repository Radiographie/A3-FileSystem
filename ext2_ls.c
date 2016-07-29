#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "ext2.h"

#define BLOCK_NUM   128
#define BLOCK_SIZE  1024

unsigned char *ptr_disk;
void scan_path(char *path, struct ext2_inode *ptr_inode, int bg_inode_table);
void recurse_path(char *path, struct ext2_dir_entry_2 *ptr_dir, int bg_inode_table);

/*
 * get absolute path and list all the file of path
 */
void recurse_path(char *path, struct ext2_dir_entry_2 *ptr_dir, int bg_inode_table)
{
    int recurse_inode = 0;
    int recurse_success = -1;
    char *end_flag = path;
    int dir_entry_size = 0;
    const char delimt_char[2] = "/";
    
    // find the sub directory we want to show
    for (dir_entry_size = 0; dir_entry_size < EXT2_BLOCK_SIZE;)
    {
        if (end_flag)
        {
            if (EXT2_FT_DIR == ptr_dir->file_type)
            {
                if (strncmp(ptr_dir->name, path, strlen(path)) == 0)
                {
                    if (recurse_success == -1)
                        recurse_success = 0;
                    recurse_inode = ptr_dir->inode;
                }
            }
        }
        //if token is null, we have got the absolute path of the file.
        else
        {
            printf("%.*s \n", ptr_dir->name_len, ptr_dir->name);
        }
        
        // get next directory entry
        dir_entry_size += ptr_dir->rec_len;
        ptr_dir = (void *)ptr_dir + ptr_dir->rec_len;
    }
    
    if (end_flag == NULL)
        exit(0);
    
    //return if no path exists
    if (recurse_success == -1 && end_flag != NULL) {
        printf("No such file or directory\n");
        exit(1);
    }
  
    end_flag = strtok(NULL, delimt_char);
    
    struct ext2_inode *inode_ptr2 = (struct ext2_inode *)(ptr_disk + EXT2_BLOCK_SIZE * bg_inode_table);
    inode_ptr2 += recurse_inode - 1;
    scan_path(end_flag, inode_ptr2, bg_inode_table);
}


/*
 * read all the inode of an inode
 */
void scan_path(char *path, struct ext2_inode *inode_ptr, int bg_inode_table)
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
            void *addr = ptr_disk + EXT2_BLOCK_SIZE * block_id;
            struct ext2_dir_entry_2 *ptr_dir = (struct ext2_dir_entry_2 *)(addr);
            recurse_path(path, ptr_dir, bg_inode_table);
        }
        i++;
    }
}

int main(int argc, char **argv)
{

    if (argc != 3)
    {
        fprintf(stderr, "Usage: ext2_ls <image file name> <path name>\n");
        exit(1);
    }
    // open disk image
    int fd = open(argv[1], O_RDWR);

    // alloc space for disk image
    ptr_disk = mmap(NULL, BLOCK_NUM * BLOCK_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (ptr_disk == MAP_FAILED)
    {
	   perror("mmap");
	   exit(1);
    }

    char *ptr_filepath = argv[2];
     
    //start of block group descriptors
    struct ext2_group_desc *gd_ptr = (struct ext2_group_desc *)(ptr_disk + EXT2_SB_OFFSET + EXT2_BLOCK_SIZE);
    //point to the first inode
    struct ext2_inode *inode_ptr = (struct ext2_inode *)(ptr_disk + EXT2_BLOCK_SIZE * gd_ptr->bg_inode_table);
    //point to the root inode
    inode_ptr++;

    // execute listing the path
    const char delimit[2] = "/";
    char *ptr_path = strtok(ptr_filepath, delimit);
    scan_path(ptr_path, inode_ptr, gd_ptr->bg_inode_table);

    return 0;
}
