typedef struct 
{
	char *address;
} DISK_MEMORY;

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include "types.h"
#include "common.h"
#include "disk.h"
#include "ext2.h"
#include "disksim.h"

#define MIN(a, b)                    ( ( a ) < ( b ) ? ( a ) : ( b ) )
#define MAX(a, b)                    ( ( a ) > ( b ) ? ( a ) : ( b ) )


/******************************************************************************/
/*																			  */
/*								EXT2_COMMAND 								  */
/*																			  */
/******************************************************************************/

/**
 * write in ext2
 * @param file The EXT2_NODE in which buffer is to be written
 * @param offset The offset after which buffer is to be written.
 * @param length The length of the file specified.
 * @param buffer The content that is to be written
 * @return The success / failure of operation.  
 */
int 
ext2_write(EXT2_NODE *file, unsigned long offset, unsigned long length, 
		const char *buffer) 
{
#if DEBUG
	printf("---------------\n");
	printf("Inside %s\n", __func__);
#endif

	BYTE 		block[MAX_BLOCK_SIZE];
	int 		i;
	EXT2_DIR_ENTRY 	*dir;
	EXT2_DIR_ENTRY 	my_entry;
	INT32 		block_num;
	DISK_OPERATIONS *disk = file->fs->disk;
	INODE 		curr_inode;


	block_num = length / MAX_BLOCK_SIZE;

#if DEBUG
	printf("block_num = %u\n", block_num);
#endif

	block_read(disk, file->location.block, block);
	dir = (EXT2_DIR_ENTRY *) block;

#if DEBUG
	printf("file->location.offset = %u\n", file->location.offset);
#endif

	my_entry = dir[file->location.offset];

	/*
	 * get the inode corresponding to my_entry.inode in curr_inode from
	 * file->fs
	 */

#if DEBUG
	printf("my_entry.inode = %u\n", my_entry.inode);
#endif

	get_inode(file->fs, my_entry.inode, &curr_inode);

#if DEBUG
	printf("curr_inode.blocks = %u\n", curr_inode.blocks);
#endif

	curr_inode.size = length;

	if (block_num < EXT2_D_BLOCKS) 
	{
		curr_inode.blocks = block_num;

#if DEBUG
		printf("curr_inode.blocks = %u\n", curr_inode.blocks);
#endif

		for (i = 0; i < block_num - 1; i++) 
		{

#if DEBUG
			printf("In for loop\n");
#endif

			curr_inode.block[i + 1] = 
				get_free_block_number(file->fs);
		}
	} 
	else 
	{
		return EXT2_ERROR;
	}

	fill_data_block(disk, &curr_inode, length, offset, buffer, block_num);

#if DEBUG
	printf("inode = %u\n", my_entry.inode);	
#endif

	insert_inode_table(file, &curr_inode, my_entry.inode);

	return EXT2_SUCCESS;
}


/**
 * ext2 read
 * @param EXT2Entry
 * @param offset
 * @param length
 * @param buffer
 * @return
 */
int 
ext2_read(EXT2_NODE *EXT2Entry, int offset, unsigned long length, 
		char *buffer) 
{

#if DEBUG
	printf("inside %s\n", __func__);
#endif

	BYTE 		block[MAX_BLOCK_SIZE];
	int 		count 			= offset / MAX_BLOCK_SIZE;
	INODE 		inodeBuffer;

#if DEBUG
	printf("inode = %u\n", EXT2Entry->entry.inode);	
#endif

	get_inode(EXT2Entry->fs, EXT2Entry->entry.inode, &inodeBuffer);

#if DEBUG
	printf("inodeBuffer->size = %u\n",inodeBuffer.size);
	printf("inodeBuffer->blocks = %u\n", inodeBuffer.blocks);
	printf("count = %u\n", count);
	printf("inodeBuffer->block[count] = %u\n", 
			inodeBuffer.block[count]);
#endif

	block_read(EXT2Entry->fs->disk, inodeBuffer.block[count], block);
	memcpy(buffer, block, MAX_BLOCK_SIZE - 1);

	if (inodeBuffer.blocks + 1 > count)
	{
		return 1;
	}
	return -1;
}

/**
 * ext2 remove directory.
 * remove the directory from parent.
 * @param parent the directory from whcih rmdir is to be removed.
 * @param rmdir the directory that is to be removed.
 * @return 
 */
int 
ext2_rmdir(EXT2_NODE *parent, EXT2_NODE *rmdir) 
{

#if DEBUG
	printf("---------------\n");
	printf("Inside %s\n", __func__);
#endif

	DISK_OPERATIONS	*disk = parent->fs->disk;
	INODE 		inodeBuffer;
	BYTE 		block[MAX_BLOCK_SIZE];
	EXT2_DIR_ENTRY 	*dir;
	INT32 		count = 1;
	INT32 		inodenum;
	INT32		groupnum;
	char 		*entryName = NULL;
	EXT2_NODE 	bufferNode;

	rmdir->fs = parent->fs;
	int 		i;
	int		bitmap_num;

	/*
	 * get inode at location rmdir->entry.inode from parent->fs in 
	 * inodeBuffer.
	 */
	get_inode(parent->fs, rmdir->entry.inode, &inodeBuffer);

	/*
	 * CHeck whether it is a directory and can be removed or not.
	 */
	if (!(inodeBuffer.mode & FILE_TYPE_DIR)) 
	{
		printf("this is not type DIR!!\n");
		return EXT2_ERROR;
	}

	/*
	 * lookup the NULL entry in bufferNode
	 */

#if DEBUG
	printf("ext2_rmdir : Before lookup entry entryName = %s\n",
			entryName);
#endif

	if (lookup_entry(parent->fs, rmdir->entry.inode, entryName, 
				&bufferNode) == EXT2_SUCCESS)
	{

#if DEBUG
		printf("ext2_rmdir : After lookup entry entryName"
			" = %s\n", entryName);
		printf("ext2_rmdir : lookup entry success\n");
#endif

		return EXT2_ERROR;
	}

	/*
	 *
	 */
	for (i = 0; i < inodeBuffer.blocks; i++) 
	{
		/*
		 * zero block.
		 */
		ZeroMemory(block, MAX_BLOCK_SIZE);
		if (i <= EXT2_D_BLOCKS) 
		{
			/*
			 * Write zero block at appropriate position.
			 */
			block_write(disk, inodeBuffer.block[i], block);

			/*
			 * increment the free block count in group descriptor.
			 */
			set_gd_free_block_count(rmdir, inodeBuffer.block[i], 
					count);

			/*
			 * increment the free block count in superblock.
			 */
			set_sb_free_block_count(rmdir, count);

			/*
			 * Read the block bitmap 
			 */
			block_read(disk, 
			(BLOCKS_PER_GROUP * GET_DATA_GROUP(inodeBuffer.block[i]) 
			 	+ BLOCK_BITMAP_BASE), 
			block);

			/*
			 * Don't know ???
			 */
			bitmap_num = (inodeBuffer.block[i] % (BLOCKS_PER_GROUP)) 
				- DATA_BLOCK_BASE;


			/*
			 * sets sero bits 
			 */
			set_zero_bit(bitmap_num, block);
			//----------------------------

			/*
			 * writes the updated bitmap table
			 */
			block_write(disk, 
			(BLOCKS_PER_GROUP * GET_DATA_GROUP(inodeBuffer.block[i]) 
			 	+ BLOCK_BITMAP_BASE), 
			block);

		} 
		else 
		{

		}
	}
	
	/*
	 * Read the block from block location in rmdir.
	 */
	block_read(disk, rmdir->location.block, block);
	dir = (EXT2_DIR_ENTRY *) block;
	dir += rmdir->location.offset;

	inodenum = dir->inode;
	groupnum = inodenum / INODES_PER_GROUP;

	ZeroMemory(dir, sizeof(EXT2_DIR_ENTRY));
	block_write(disk, rmdir->location.block, block);
	dir->name[0] = DIR_ENTRY_FREE;
	set_entry(parent->fs, &rmdir->location, dir);


	ZeroMemory(&inodeBuffer, sizeof(INODE));
	insert_inode_table(parent, &inodeBuffer, inodenum);

	block_read(disk, groupnum * BLOCKS_PER_GROUP + INODE_BITMAP_BASE, block);
	bitmap_num = (inodenum % INODES_PER_GROUP) - 1;
	set_zero_bit(bitmap_num, block);
	block_write(disk, 
	(groupnum * BLOCKS_PER_GROUP + INODE_BITMAP_BASE), 
	block);


	set_sb_free_inode_count(rmdir, count);
	set_gd_free_inode_count(rmdir, inodenum, count);

	return EXT2_SUCCESS;
}

/**
 * ext2 remove file
 * @param parent
 * @param rmfile
 * @return
 */
int ext2_remove(EXT2_NODE *parent, EXT2_NODE *rmfile) 
{

#if DEBUG
	printf("---------------\n");
	printf("Inside %s\n", __func__);
#endif

	DISK_OPERATIONS *disk = parent->fs->disk;
	INODE 		inodeBuffer;
	BYTE 		block[MAX_BLOCK_SIZE];
	EXT2_DIR_ENTRY 	*dir;
	INT32 		count 			= 1;
	INT32 		inodenum;
	INT32		groupnum;

	rmfile->fs = parent->fs;
	INT32 i, bitmap_num;

	get_inode(parent->fs, rmfile->entry.inode, &inodeBuffer);

	if (inodeBuffer.mode & FILE_TYPE_DIR) 
	{
		printf("type DIR!!\n");
		return EXT2_ERROR;
	}


	for (i = 0; i < inodeBuffer.blocks; i++) 
	{
		ZeroMemory(block, MAX_BLOCK_SIZE);
		if (i <= EXT2_D_BLOCKS) 
		{
			block_write(disk, inodeBuffer.block[i], block);
			set_gd_free_block_count(rmfile, inodeBuffer.block[i], 
					count);
			set_sb_free_block_count(rmfile, count);

			block_read(disk, BLOCKS_PER_GROUP * 
					GET_DATA_GROUP(inodeBuffer.block[i]) + 
					BLOCK_BITMAP_BASE, block);

			bitmap_num = (inodeBuffer.block[i] % (BLOCKS_PER_GROUP)) 
					- DATA_BLOCK_BASE;

			set_zero_bit(bitmap_num, block);

			block_write(disk, BLOCKS_PER_GROUP * 
					GET_DATA_GROUP(inodeBuffer.block[i]) + 
					BLOCK_BITMAP_BASE, block);

		} 
		else 
		{

		}
	}

	block_read(disk, rmfile->location.block, block);
	dir = (EXT2_DIR_ENTRY *) block;
	dir += rmfile->location.offset;

	inodenum = dir->inode;
	groupnum = inodenum / INODES_PER_GROUP;

	ZeroMemory(dir, sizeof(EXT2_DIR_ENTRY));
	block_write(disk, rmfile->location.block, block);
	dir->name[0] = DIR_ENTRY_FREE;
	set_entry(parent->fs, &rmfile->location, dir);


	ZeroMemory(&inodeBuffer, sizeof(INODE));
	insert_inode_table(parent, &inodeBuffer, inodenum);

	block_read(disk, groupnum * BLOCKS_PER_GROUP + INODE_BITMAP_BASE, block);
	bitmap_num = (inodenum % INODES_PER_GROUP) - 1;
	set_zero_bit(bitmap_num, block);
	block_write(disk, groupnum * BLOCKS_PER_GROUP + INODE_BITMAP_BASE, 
			block);


	set_sb_free_inode_count(rmfile, count);
	set_gd_free_inode_count(rmfile, inodenum, count);

	return EXT2_SUCCESS;
}

/**
 * ext2 make directory.
 * @param parent
 * @param entryName
 * @param retEntry
 * @return 
 */
int 
ext2_mkdir(EXT2_NODE *parent, const char *entryName, EXT2_NODE *retEntry) 
{

#if DEBUG
	printf("---------------\n");
	printf("Inside %s\n", __func__);
#endif

	EXT2_NODE 	dotNode;
	EXT2_NODE	dotdotNode;
	char 		name[MAX_NAME_LENGTH] = {'\0'};
	int 		result;
	int 		free_inode;
	int		free_data_block;
	int		count;
	INODE 		inode_entry;

	strcpy((char *) name, entryName);
	
#if DEBUG
	printf("------------\n");
	printf("in ext2_mkdir name = %s\n", name);
#endif

	/*
	 * Convert the name to upper case and check on the length of the 
	 */
	if ((format_name((char *) name)) == EXT2_ERROR)
	{

#if DEBUG
		printf("%s : format_name error\n", __func__);
#endif

		return EXT2_ERROR;
	}

#if DEBUG
	printf("after format_name, name = %s\n", name);
#endif

	/*
	 * Get free inode number
	 */
	free_inode = get_free_inode_number(parent->fs);
	
	/*
	 * Get the free data block in parent's filesystem
	 */
	free_data_block = get_free_block_number(parent->fs);

	count = -1;

	/*
	 * Get the filesystem
	 */ 
	retEntry->fs = parent->fs;
	
	/*
	 * Update the super block's free block count.
	 */
	set_sb_free_block_count(retEntry, count);

	/*
	 * Update the super block's free inode count.
	 */
	set_sb_free_inode_count(retEntry, count);

	/*
	 * update gd
	 */
	set_gd_free_block_count(retEntry, free_data_block, count);

	/*
	 * update gd
	 */
	set_gd_free_inode_count(retEntry, free_inode, count);

	ZeroMemory(retEntry, sizeof(EXT2_NODE));
	memcpy(retEntry->entry.name, name, MAX_NAME_LENGTH);

#if DEBUG
	printf("in ext2_mkdir retEntry->entry.name = %s\n", 
			retEntry->entry.name);
#endif

	retEntry->entry.name_len = strlen((char *) retEntry->entry.name);
	retEntry->entry.inode = free_inode;
	retEntry->fs = parent->fs;

	/*
	 * insert the retEntry in parent
	 */
	result = insert_entry(parent, retEntry, FILE_TYPE_DIR);

	if (result == EXT2_ERROR)
	{

#if DEBUG
		printf("%s : insert_entry error\n", __func__);
#endif

		return EXT2_ERROR;
	}

	ZeroMemory(&inode_entry, sizeof(INODE));
	inode_entry.block[0] = free_data_block;
	inode_entry.mode = USER_PERMITION | FILE_TYPE_DIR;
	inode_entry.blocks = 1;
	inode_entry.size = 0;

	/*
	 * insert new inode entry in inode table.
	 */
	insert_inode_table(parent, &inode_entry, free_inode);

	ZeroMemory(&dotNode, sizeof(EXT2_NODE));
	memset(dotNode.entry.name, 0x20, MAX_NAME_LENGTH);

	dotNode.entry.name[0] = '.';
	dotNode.fs = retEntry->fs;
	dotNode.entry.inode = retEntry->entry.inode;
	/*
	 * insert .  in new entry.
	 */
	insert_entry(retEntry, &dotNode, FILE_TYPE_DIR);


	ZeroMemory(&dotdotNode, sizeof(EXT2_NODE));
	memset(dotdotNode.entry.name, 0x20, MAX_NAME_LENGTH);
	dotdotNode.entry.name[0] = '.';
	dotdotNode.entry.name[1] = '.';
	dotdotNode.entry.inode = parent->entry.inode;
	dotdotNode.fs = retEntry->fs;

	/*
	 * insert .. in new entry.
	 */
	insert_entry(retEntry, &dotdotNode, FILE_TYPE_DIR);

	return EXT2_SUCCESS;
}

/**
 * EXT2 read directory.
 * This reads the user made directory entries and adds them in the list.
 * @param dir The Node that contains the directory to be read.
 * @param adder The function pointer needed to add a node to the list.
 * @param list The list that contains the directory entries.
 * @return Success/ Fail value.
 */
int 
ext2_read_dir(EXT2_NODE *dir, EXT2_NODE_ADD adder, void *list) 
{

#if DEBUG
	printf("---------------\n");
	printf("Inside %s\n", __func__);
#endif


	BYTE 	block[MAX_BLOCK_SIZE];
	INODE 	*inodeBuffer;
	int 	i; 
	int	result;
       	int	block_num;

	/*
	 * Get the group number.
	 * But why?
	 * Not using it anywhere in the function though.
	 */
	inodeBuffer = (INODE *) malloc(sizeof(INODE));

	ZeroMemory(block, MAX_BLOCK_SIZE);
	ZeroMemory(inodeBuffer, sizeof(INODE));

	/*
	 * Get the inode corresponding to dir->entry.inode from dir->fs->disk 
	 * into inodeBuffer
	 */
	result = get_inode(dir->fs, dir->entry.inode, inodeBuffer);

	if (result == EXT2_ERROR)
	{
		return EXT2_ERROR;
	}

	/*
	 * This loop has no confirmed meaning.
	 * For each block number that inodeBuffer has,
	 * It just reads the block, converts it to the EXT2_DIR_ENTRY array,
	 * and reads all entries from the directory entry array into list.
	 * *******Everytime, inodeBuffer->blocks = 1 ***********
	 * *******Loop gets executed only once ************
	 */
	for (i = 0; i < inodeBuffer->blocks; ++i) 
	{
		/*
		 * For each index in present in inodeBuffer->block[],
		 * get the block number stored at that index.
		 */
		block_num = get_data_block_at_inode(dir->fs, *inodeBuffer, i);

		/*
		 * Read the block of data from dir->fs->disk which has block 
		 * number equal to block num into the location given by 
		 * block(parameter).
		 */
		block_read(dir->fs->disk, block_num, block);

		/*
		 * block now has the block of data corresponding to
		 * block_num.
		 * The block is converted to EXT2_DIR_ENTRY array and then,
		 * all the directory entries that are present in the
		 * directory entry array and are user made are added to the list.
		 */
		read_dir_from_block(dir->fs, block, adder, list);

	}

	return EXT2_SUCCESS;
}

/*
 * ext2_format.
 * The disk is formatted to store data.
 * The internal partitions on the disk are initialized.
 * @param disk The disk on which operations are to be done.
 */
int 
ext2_format(DISK_OPERATIONS *disk) 
{

#if DEBUG
	printf("---------------\n");
	printf("Inside %s\n", __func__);
#endif

	EXT2_SUPER_BLOCK 		sb;
	EXT2_GROUP_DESCRIPTOR_TABLE 	gdt;
	int 				i	= 0;
	
	printf("sizeof(EXT2_DIR_ENTRY) = %lu\n", sizeof(EXT2_DIR_ENTRY));

	if ((fill_super_block(&sb)) != EXT2_SUCCESS)
	{
		return EXT2_ERROR;
	}
	if ((fill_descriptor_table(&gdt, &sb)) != EXT2_SUCCESS)
	{
		return EXT2_ERROR;
	}

#if DEBUG
	printf("i = %d\n", i);
#endif

	init_inode_bitmap(disk, i);

	for (i = 0; i < NUMBER_OF_GROUPS; i++) 
	{
		sb.block_group_number = i;
		init_super_block(disk, &sb, i);
		init_gdt(disk, &gdt, i);
		init_block_bitmap(disk, i);
	}
	PRINTF("max inode count                : %d\n", sb.max_inode_count);
	PRINTF("total block count              : %u\n", sb.block_count);
	PRINTF("byte size of inode structure   : %u\n", sb.inode_structure_size);
	PRINTF("block byte size                : %u\n", MAX_BLOCK_SIZE);
	PRINTF("total sectors count            : %u\n", NUMBER_OF_SECTORS);
	PRINTF("sector byte size               : %u\n", MAX_SECTOR_SIZE);
	PRINTF("\n");
	create_root(disk, &sb);

	return EXT2_SUCCESS;
}

/**
 * ext2 sync
 * @param disk the disk that is to be synced
 * @return Success / Failure value
 */
int
ext2_sync(DISK_OPERATIONS *disk)
{
	int 	result;
	FILE 	*fp 	= fopen("storage/diskimg", "w");

	if (fp == NULL)
	{
		fprintf(stderr, "Error in file openeing\n");
		exit(EXIT_FAILURE);
	}

	result = fwrite((((DISK_MEMORY *)disk->pdata)->address), 
			(NUMBER_OF_SECTORS * MAX_SECTOR_SIZE),
			1, fp);

#if DEBUG
	printf("NUMBER_OF_SECTORS = %u\n", NUMBER_OF_SECTORS);
	printf("MAX_SECTOR_SIZE = %u\n", MAX_SECTOR_SIZE);
	printf("result = %u\n", result);
#endif

	if (result == 1)
	{
		result = EXT2_SUCCESS;	
	}
	else
	{
		result = EXT2_ERROR;
	}

	fclose(fp);
	return result;
	
}

/**
 * ext2 lookup
 * @param parent The parent directory in which we have to look.
 * @entryName The name of the directory that is to be searched.
 * @retEntry The entry that was found.
 * @return Success /  failure value. 
 */
int 
ext2_lookup(EXT2_NODE *parent, const char *entryName, EXT2_NODE *retEntry) 
{

#if DEBUG
	printf("---------------\n");
	printf("Inside %s\n", __func__);
#endif

	char 	formattedName[MAX_NAME_LENGTH]	= {0,};

	strncpy(formattedName, entryName, MAX_NAME_LENGTH);
	format_name(formattedName);

	int result = lookup_entry(parent->fs, parent->entry.inode, formattedName,
			retEntry);

	return result;
}

/**
 * ext2 create.
 * Create a new EXT2_NODE entry in parent entry.
 * @param parent The parent EXT2_NODE in which a new node is to be added.
 * @param entryName The name of the file to be created.
 * @param retEntry The return EXT2_NODE that is to be added.
 * @return success / failure of operation.
 */
int ext2_create(EXT2_NODE *parent, const char *entryName, EXT2_NODE *retEntry) 
{

#if DEBUG
	printf("---------------\n");
	printf("Inside %s\n", __func__);
#endif

	EXT2_DIR_ENTRY	dir_entry;
	char 		name[MAX_NAME_LENGTH]	= {0,};
	INODE 		inode_entry;
	UINT32 		free_inode;
	UINT32 		free_data_block;
	INT32 		count;
	
	/*
	 * If there is no free inode left then throw error.
	 */
	if ((parent->fs->gd.free_inodes_count) == 0) return EXT2_ERROR;

	strncpy(name, entryName, MAX_NAME_LENGTH);

	if (format_name(name) == EXT2_ERROR) return EXT2_ERROR;

	ZeroMemory(retEntry, sizeof(EXT2_NODE));

	memcpy(retEntry->entry.name, entryName, MAX_NAME_LENGTH);

	/*
	 * IF name is already present then throw error..
	 */
	if (ext2_lookup(parent, name, retEntry) == EXT2_SUCCESS) 
	{
		return EXT2_ERROR;
	}

	retEntry->fs = parent->fs;

	/*
	 * get free inode and data block number.
	 */
	free_inode = get_free_inode_number(parent->fs);
	free_data_block = get_free_block_number(parent->fs);

	count = -1;

	/*
	 * set free inode and data block count.
	 */
	set_sb_free_block_count(retEntry, count);
	set_sb_free_inode_count(retEntry, count);

	set_gd_free_block_count(retEntry, free_data_block, count);
	set_gd_free_inode_count(retEntry, free_inode, count);

	ZeroMemory(&inode_entry, sizeof(INODE));

	/*
	 * Set the inode elements.
	 */
	inode_entry.block[0] = free_data_block;
	inode_entry.mode = USER_PERMITION | 0x2000;
	inode_entry.blocks = 1;
	inode_entry.size = 0;

	/*
	 * insert in inode table at free_inode position.
	 */
	insert_inode_table(parent, &inode_entry, free_inode);

	/*
	 * Set a new directory entry values. 
	 */
	ZeroMemory(&dir_entry, sizeof(EXT2_DIR_ENTRY));
	memcpy(retEntry->entry.name, name, MAX_NAME_LENGTH);
	retEntry->entry.name_len = my_strnlen(retEntry->entry.name);
	retEntry->entry.inode = free_inode;

	/*
	 * insert the EXT2_NODE
	 */
	insert_entry(parent, retEntry, inode_entry.mode);

	return EXT2_SUCCESS;
}


/******************************************************************************/
/*																			  */
/*								init function 								  */
/*																			  */
/******************************************************************************/

/**
 * fill super block
 * @param sb the super block that is to be filled
 * @return The Success / failure value of operation
 */
int fill_super_block(EXT2_SUPER_BLOCK *sb) 
{

#if DEBUG
	printf("---------------\n");
	printf("Inside %s\n", __func__);
#endif

	ZeroMemory(sb, sizeof(EXT2_SUPER_BLOCK));
	sb->max_inode_count = NUMBER_OF_INODES;
	sb->first_data_block_each_group = 1 + BLOCK_NUM_GDT_PER_GROUP + 1 + 1 +
		((INODES_PER_GROUP * INODE_SIZE + (MAX_BLOCK_SIZE - 1)) /
		 MAX_BLOCK_SIZE);
	sb->block_count = NUMBER_OF_BLOCK;
	sb->reserved_block_count = sb->block_count / 100 * 5;
	sb->free_block_count = NUMBER_OF_BLOCK - 
		(sb->first_data_block_each_group) - 1;
	sb->free_inode_count = NUMBER_OF_INODES - 11;
	sb->first_data_block = SUPER_BLOCK_BASE;
	sb->log_block_size = 0;
	sb->log_fragmentation_size = 0;
	sb->block_per_group = BLOCKS_PER_GROUP;
	sb->fragmentation_per_group = 0;
	sb->inode_per_group = NUMBER_OF_INODES / NUMBER_OF_GROUPS;
	sb->magic_signature = 0xEF53;
	sb->errors = 0;
	sb->first_non_reserved_inode = 11;
	sb->inode_structure_size = 128;

	return EXT2_SUCCESS;
}

/**
 * fill descriptor table
 * @param gdb
 * @param sb
 * @return
 */
int 
fill_descriptor_table(EXT2_GROUP_DESCRIPTOR_TABLE *gdb, EXT2_SUPER_BLOCK *sb) 
{
	EXT2_GROUP_DESCRIPTOR gd;
	int i;

	ZeroMemory(gdb, sizeof(EXT2_GROUP_DESCRIPTOR_TABLE));
	ZeroMemory(&gd, sizeof(EXT2_GROUP_DESCRIPTOR));

	for (i = 0; i < NUMBER_OF_GROUPS; i++) 
	{
		gd.start_block_of_block_bitmap = BLOCK_BITMAP_BASE + 
			(BLOCKS_PER_GROUP * i);
		gd.start_block_of_inode_bitmap = BLOCK_BITMAP_BASE + 
			(BLOCKS_PER_GROUP * i) + 1;
		gd.start_block_of_inode_table = BLOCK_BITMAP_BASE + 
			(BLOCKS_PER_GROUP * i) + 1 + 1;
		gd.free_blocks_count = 
			(sb->free_block_count / NUMBER_OF_GROUPS);
		gd.free_inodes_count = 
			(((sb->free_inode_count) + 10) / NUMBER_OF_GROUPS) * 2;
		gd.directories_count = 0;
		memcpy(&gdb->group_descriptor[i], &gd, 
				sizeof(EXT2_GROUP_DESCRIPTOR));
	}
	return EXT2_SUCCESS;
}

/**
 * init_super_block.
 * did not understand why.
 * @param disk The DISK_OPERATIONS structure where the bytes from sb are written
 * 	       and then read.
 * @param sb The EXT2_SUPER_BLOCK which is to be initialized.
 * @param group_number The group number that acts as an index to get the 
 * 		       starting block number that is to be written and read.
 */
int 
init_super_block(DISK_OPERATIONS *disk, 
		EXT2_SUPER_BLOCK *sb, 
		UINT32 group_number) 
{

#if DEBUG
	printf("---------------\n");
	printf("Inside %s\n", __func__);
#endif

	BYTE block[MAX_BLOCK_SIZE];
	ZeroMemory(block, sizeof(block));

	/*
	 * copy from sb to block.
	 */
	memcpy(block, sb, sizeof(block));

	/*
	 * write from block to disk.
	 * (i.e. data of sb to disk)
	 */
	block_write(disk, SUPER_BLOCK_BASE + (group_number * BLOCKS_PER_GROUP), 
			block);
	ZeroMemory(block, sizeof(block));

	/*
	 * Read from disk to block.
	 */
	block_read(disk, SUPER_BLOCK_BASE + (group_number * BLOCKS_PER_GROUP), 
			block);

	return EXT2_SUCCESS;
}

/*
 * init_gdt.
 * @param disk The disk in which the bytes are to be copied.
 * @param gdt The EXT2_GROUP_DESCRIPTOR_TABLE from which bytes are to be copied.
 * @param group_number The current group number.
 */
int 
init_gdt(DISK_OPERATIONS *disk, 
	EXT2_GROUP_DESCRIPTOR_TABLE *gdt, 
	UINT32 group_number) 
{

#if DEBUG
	printf("---------------\n");
	printf("Inside %s\n", __func__);
#endif

	BYTE block[MAX_BLOCK_SIZE];
	int gdt_block_num;
	EXT2_GROUP_DESCRIPTOR_TABLE *gdt_read = gdt;

	for (gdt_block_num = 0; 
			(gdt_block_num < BLOCK_NUM_GDT_PER_GROUP); 
			gdt_block_num++) 
	{
		ZeroMemory(block, sizeof(block));

		/*
		 * copy the bytes from gdt_read to block.
		 * at most MAX_BLOCK_SIZE bytes are copied.
		 * With each iteartion, the address to be copied from increases 
		 * by MAX_BLOCK_SIZE.
		 */
		memcpy(block, gdt_read + (gdt_block_num * MAX_BLOCK_SIZE), 
				sizeof(block));

		/*
		 * Write from block to disk.
		 * The block number is given by middle parameter.
		 */
		block_write(disk, GDT_BASE + (group_number * BLOCKS_PER_GROUP) + 
				gdt_block_num, block);
	}
	return EXT2_SUCCESS;
}

/*
 * init_block_bitmap.
 * Initialize the block_bitmap on disk.
 * This creates an initialized block and then copies it to wherever the 
 * bitmap is to be stored on disk.
 */
int 
init_block_bitmap(DISK_OPERATIONS *disk, 
		UINT32 group_number) 
{

#if DEBUG
	printf("---------------\n");
	printf("Inside %s\n", __func__);
#endif

	BYTE block[MAX_BLOCK_SIZE];
	ZeroMemory(block, sizeof(block));
	int i;

	/*
	 * This loop will set each byte in block to 0xFF,
	 * until block[(DATA_BLOCK_BASE / 8)] is reached.
	 */
	for (i = 0; i < DATA_BLOCK_BASE; i++)
	{
		set_bit(i, block);
	}

	/*
	 * Write block to disk.
	 * The block number to be written is given by middle parameter.
	 */
	block_write(disk, BLOCK_BITMAP_BASE + (group_number * BLOCKS_PER_GROUP), 
			block);

	return EXT2_SUCCESS;
}

/**
 * initialize inode bitmap
 * @param disk
 * @param group number
 * @return
 */

int 
init_inode_bitmap(DISK_OPERATIONS *disk, UINT32 group_number) 
{

#if DEBUG
	printf("---------------\n");
	printf("Inside %s\n", __func__);

	printf("group number = %d\n", group_number);
#endif

	BYTE 		block[MAX_BLOCK_SIZE];

	ZeroMemory(block, sizeof(block));

	int 		i;

	for (i = 0; i < 10; i++)
	{
		set_bit(i, block);
	}

	block_write(disk, INODE_BITMAP_BASE + (group_number * BLOCKS_PER_GROUP), 
			block);

	return EXT2_SUCCESS;
}

/**
 * initialize data block.
 * @param disk
 * @param group number
 * @return
 */
int 
init_data_block(DISK_OPERATIONS *disk, UINT32 group_number) 
{

#if DEBUG
	printf("---------------\n");
	printf("Inside %s\n", __func__);
#endif

	BYTE block[MAX_BLOCK_SIZE];
	ZeroMemory(block, sizeof(block));
	block_write(disk, DATA_BLOCK_BASE + (group_number * BLOCKS_PER_GROUP), 
			block);
	return EXT2_SUCCESS;
}

/**
 * create root.
 */
int 
create_root(DISK_OPERATIONS *disk, 
	EXT2_SUPER_BLOCK *sb) 
{

#if DEBUG
	printf("---------------\n");
	printf("Inside %s\n", __func__);
#endif

	BYTE block[MAX_BLOCK_SIZE];
	SECTOR rootBlock = 0;
	EXT2_DIR_ENTRY *entry;
	EXT2_GROUP_DESCRIPTOR *gd;
	EXT2_SUPER_BLOCK *sb_read;
	QWORD block_num_per_group = BLOCKS_PER_GROUP;
	INODE ip;
	int gi;

	ZeroMemory(block, MAX_BLOCK_SIZE);
	entry = (EXT2_DIR_ENTRY *) block;

	memset(entry->name, 0x20, MAX_NAME_LENGTH);
	entry->name[0] = '.';
	entry->name[1] = '.';
	entry->inode = NUM_OF_ROOT_INODE;
	entry->name_len = 2;

	entry++;
	memset(entry->name, 0x20, MAX_NAME_LENGTH);
	entry->name[0] = '.';
	entry->inode = NUM_OF_ROOT_INODE;
	entry->name_len = 1;

	entry++;
	entry->name[0] = DIR_ENTRY_NO_MORE;

	rootBlock = 1 + sb->first_data_block_each_group;

	/*
	 * Write block at block number given by rootblock in disk.
	 */
	block_write(disk, rootBlock, block);

	sb_read = (EXT2_SUPER_BLOCK *) block;
	for (gi = 0; gi < NUMBER_OF_GROUPS; gi++) 
	{
		/*
		 * read from disk in block.
		 * The block number to be read from is given by middle parameter.
		 */
		block_read(disk, block_num_per_group * gi + SUPER_BLOCK_BASE, 
				block);
		sb_read->free_block_count--;

		/*
		 * Write from block into disk.
		 *
		 */
		block_write(disk, block_num_per_group * gi + SUPER_BLOCK_BASE, 
				block);
	}

	gd = (EXT2_GROUP_DESCRIPTOR *) block;

	/*
	 * Read from disk in block.
	 * The block number to be read from is given by middle paramter.
	 */
	block_read(disk, GDT_BASE, block);

	gd->free_blocks_count--;
	gd->directories_count = 1;
	for (gi = 0; gi < NUMBER_OF_GROUPS; gi++)
	{
		/*
		 * Write from block to disk. 
		 */
		block_write(disk, block_num_per_group * gi + GDT_BASE, block);
	}
	
	/*
	 * read from disk into block.
	 * The block containing the block_bitmap is read into block.
	 */
	block_read(disk, BLOCK_BITMAP_BASE, block);

	/*
	 *
	 */
	set_bit(rootBlock, block);
	block_write(disk, BLOCK_BITMAP_BASE, block);

	ZeroMemory(block, MAX_BLOCK_SIZE);
	ip.mode = 0x1FF | 0x4000;
	ip.size = 0;
	ip.blocks = 1;
	ip.block[0] = DATA_BLOCK_BASE;

	/*
	 *
	 */
	memcpy(block + sizeof(INODE), &ip, sizeof(INODE));
	block_write(disk, INODE_TABLE_BASE, block);

	return EXT2_SUCCESS;
}


/******************************************************************************/
/*																			  */
/*								read function 								  */
/*																			  */
/******************************************************************************/

/**
 * read_superblock.
 * read superblock in root.
 */
int 
read_superblock(EXT2_FILESYSTEM *fs, EXT2_NODE *root) 
{

#if DEBUG
	printf("---------------\n");
	printf("Inside %s\n", __func__);
#endif

	BYTE block[MAX_BLOCK_SIZE];
	EXT2_DIR_ENTRY entry;
	EXT2_DIR_ENTRY_LOCATION location;
	char name[MAX_NAME_LENGTH];

	if (fs == NULL || fs->disk == NULL) 
	{
		WARNING("DISK OPERATIONS : %p\nEXT2_FILESYSTEM : %p\n", fs, 
				fs->disk);
		return EXT2_ERROR;
	}

	/*
	 * Read the superblock from disk into block.
	 */
	if (block_read(fs->disk, SUPER_BLOCK_BASE, block))
	{
		return EXT2_ERROR;
	}

	/*
	 * Check whether after the read, superblock has been correctly read,
	 * by validating the block for superblock.
	 */
	if (validate_sb(block))
	{
		return EXT2_ERROR;
	}

	/*
	 * Copy the superblock that was read from fs->disk into fs->sb.
	 * That is copy the fs->disk's superblock in filesystem's superblock.
	 */
	memcpy(&fs->sb, block, sizeof(block));

	ZeroMemory(block, sizeof(block));

	/*
	 * Read the group descriptor from GDT_BASE at fs->disk in block. 
	 */
	block_read(fs->disk, GDT_BASE, block);

	/*
	 * Copy the group descriptor read in block into fs->gd.
	 * I.E. into the filesystem's group descriptor.
	 */
	memcpy(&fs->gd, block, sizeof(EXT2_GROUP_DESCRIPTOR));

	ZeroMemory(block, sizeof(MAX_BLOCK_SIZE));

	/*
	 * Read the root block from fs->disk into block.
	 */
	if (read_root_block(fs, block))
	{
		return EXT2_ERROR;
	}

	ZeroMemory(root, sizeof(EXT2_NODE));

	/*
	 * Copy the bytes from block to root->entry.
	 * Only EXT2_DIR_ENTRY size is transferred.
	 */
	memcpy(&root->entry, block, sizeof(EXT2_DIR_ENTRY));

	memset(name, SPACE_CHAR, sizeof(name));

	/*
	 * Set values for entry.
	 * entry is a EXT2_DIR_ENTRY.
	 */
	entry.inode = 2;
	entry.name_len = 1;
	entry.name[0] = '/';

	/*
	 * Set values for location.
	 */
	location.group = 0;
	location.block = DATA_BLOCK_BASE;
	location.offset = 0;

	/*
	 * Set the values in root and return.
	 */
	root->fs = fs;
	root->entry = entry;
	root->location = location;

	return EXT2_SUCCESS;
}

/**
 * read root block.
 * @param fs The filesystem structure from which the rootblock is to be read.
 * @param block The buffer in which the rootblock is to be read.
 * @return The success/ failure of operation.
 */
int read_root_block(EXT2_FILESYSTEM *fs, BYTE *block) 
{

#if DEBUG
	printf("---------------\n");
	printf("Inside %s\n", __func__);
#endif

	UINT32 inode = 2;
	INODE inodeBuffer;
	SECTOR rootBlock;

	/*
	 * read the inode corresponding to the inode passed in argument from
	 * fs->disk into inodeBuffer.
	 */
	get_inode(fs, inode, &inodeBuffer);

	/*
	 * get the rootBlock NUmber.
	 */
	rootBlock = inodeBuffer.block[0];

	/*
	 * Read the rootblock from fs->disk into block.
	 * Now block has the rootblock.
	 */
	return block_read(fs->disk, rootBlock, block);
}


/**
 * Read dir from block.
 * This function typecasts the block into EXT2_DIR_ENTRY array and then adds all
 * user made directory entries into the list.
 * @param fs The filesystem that has the disk.
 * @param block The block which is to be typecastd to directory entry and then
 * 		names are to be read from it.
 * @param adder The function that adds the EXT2_NODE to the list.
 * @param list The list to which nodes are to be added.
 * @return EXT2_SUCCESS
 */
int 
read_dir_from_block(EXT2_FILESYSTEM *fs, BYTE *block, EXT2_NODE_ADD adder, 
		void *list) 
{

#if DEBUG
	printf("---------------\n");
	printf("Inside %s\n", __func__);
#endif

	EXT2_DIR_ENTRY 	*dir_entry;
	EXT2_NODE 	node;

	/*
	 * How does this work?
	 * This converts block into array of dir_entry.
	 * So this array of dir_entry can have maximum (1024/32) = 32 entries.
	 */

	dir_entry = (EXT2_DIR_ENTRY *) block;
	UINT i = 0;

	/*
	 * This loop goes over all entries in a directory.
	 */
	while (dir_entry[i].name[0] != DIR_ENTRY_NO_MORE) 
	{
		/*
		 * This condition basically adds every entry in the directory
		 * that is not '.' and ".." into a node.
		 * Then adds the node to the list which is a SHELL_ENTRY_LIST. 
		 */
		if ((dir_entry[i].name[0] != '.') && 
			(dir_entry[i].name[0] != DIR_ENTRY_FREE)) 
		{
			node.entry = dir_entry[i];
			node.fs = fs;

			/*
			 * location has group_number->block_number->offset.
			 * So we can find the exact location of an entry.
			 */
			node.location.offset = i;
			adder(list, &node);
		}
		i++;
	}
	/*
	 * So now, the list contains all the entries in the directory except
	 * "." and ".." 
	 */

	return EXT2_SUCCESS;
}


/******************************************************************************/
/*																			  */
/*								get function 								  */
/*																			  */
/******************************************************************************/

/**
 * get inode.
 * copy the inode corresponding to inode_num from fs->disk into inodeBuffer.
 * This is done by using the INODE_TABLE_BASE and inode_num.
 * The appropriate location is found and in the disk and then copied into
 * inodeBuffer.
 * @param fs The EXT2_FILESYSTEM pointer from whose disk the data is to be 
 * 	     copied.
 * @param inode_num The inode number in question.
 * @param inodeBuffer The inode buffer that will hold the data after copying.
 * @return The value depending upon success of failure.
 */
int 
get_inode(EXT2_FILESYSTEM *fs, UINT32 inode_num, INODE *inodeBuffer) 
{

#if DEBUG
	printf("---------------\n");
	printf("Inside %s\n", __func__);
#endif

	int	result	= 0;
	BYTE block[MAX_BLOCK_SIZE];
	inode_num--;

#if DEBUG
	printf("fs->disk = %u\n", fs->disk);
#endif
	
	/*
	 * Get the group number.
	 * We can get it by current inode_num / INODES_PER_GROUP.
	 * Suppose inode_num-- = 13446.
	 * INODES_PER_GROUP = (NUMBER_OF_INODES / NUMBER_OF_GROUPS)
	 * INODES_PER_GROUP = (NUMBER_OF_BLOCK / 2) = (262145 / 2) = 131072.5.
	 * 131072.5 / 32 = 4096.
	 *
	 * 13446 / 4096 = 3.
	 */
	UINT32 group_number = inode_num / INODES_PER_GROUP;

	/*
	 * group_inode_offset = 13446 % 4096 = 1158.
	 *
	 * Thus the current inode lies in group number 3, at the 1158 position.
	 */
	UINT32 group_inode_offset = inode_num % INODES_PER_GROUP;

	/*
	 * INODES_PER_BLOCK = 8.
	 * block number = 1158 / 8 = 144.
	 */
	UINT32 block_number = group_inode_offset / INODES_PER_BLOCK;

	/*
	 * block inode offset = 1158 % 8 = 6.
	 *
	 * Thus the current inode lies in block number 144, at position 6.
	 */
	UINT32 block_inode_offset = group_inode_offset % INODES_PER_BLOCK;

	/*
	 * INODE_TABLE_BASE = 5.
	 * BLOCKS_PER_GROUP = 8192.
	 * group_number * BLOCKS_PER_GROUP = 3 * 8192 = 24576.
	 * 24576 + 144 = 24720. 
	 * This is the block number from the start of the disk.
	 * INODE_TABLE_BASE + 24720 = 24725.
	 * The block is to be read starting from this position after the 
	 * INODE_TABLE_BASE.
	 * This bytes is read into block from position 24725 in fs->disk.
	 */

#if 0
	printf("BOOT_BLOCK_BASE = %u\n", BOOT_BLOCK_BASE);
	printf("SUPER_BLOCK_BASE = %u\n", SUPER_BLOCK_BASE);
	printf("GDT_BASE = %u\n", GDT_BASE);
	printf("BLOCK_NUM_GDT_PER_GROUP = %u\n", BLOCK_NUM_GDT_PER_GROUP);
	printf("BLOCK_BITMAP_BASE = %u\n", BLOCK_BITMAP_BASE);
	printf("INODE_BITMAP_BASE = %u\n", INODE_BITMAP_BASE);
	printf("INODE_TABLE_BASE = %u\n", INODE_TABLE_BASE);
	printf("BLOCK_NUM_IT_PER_GROUP = %u\n", BLOCK_NUM_IT_PER_GROUP);
	printf("DATA_BLOCK_BASE = %u\n", DATA_BLOCK_BASE);
#endif

	result = block_read(fs->disk, 
	(INODE_TABLE_BASE + (group_number * BLOCKS_PER_GROUP) + block_number),
			block);

	/*
	 * INODE_SIZE = 128.
	 * INODE_SIZE * block_inode_offset = 128 * 6 = 768.
	 * This will give the position of inode in the current block, 
	 * in terms of bytes.
	 * Copy the inode from that position in the block block into inodeBuffer.
	 */
	memcpy(inodeBuffer, block + (INODE_SIZE * (block_inode_offset)), 
			sizeof(INODE));

	return result;
}

/**
 * lookup entry.
 * get new entry in block array
 * @param fs The filsystem in which we have to get the new entry.
 * @param inode The inode number corresponding to which we have to find the new 
 * 		entry.
 * @param name The name that gives a starting point to be searched. 
 * @param retEntry The entry structure that is to be allocated.
 * @return EXT2_SUCCESS / EXT2_ERROR based on operation.
 */
int 
lookup_entry(EXT2_FILESYSTEM *fs, int inode, char *name, EXT2_NODE *retEntry) 
{

#if DEBUG
	printf("---------------\n");
	printf("Inside %s\n", __func__);
#endif


	BYTE 		block[MAX_BLOCK_SIZE];
	INODE 		inodeBuffer;
	EXT2_DIR_ENTRY 	*entry;
	INT32 		lastEntry 		= 
		MAX_BLOCK_SIZE / sizeof(EXT2_DIR_ENTRY) - 1;

#if DEBUG
	printf("%s : lastEntry = %u\n", __func__, lastEntry);
#endif

	UINT32 		i;
	UINT32		result;
	UINT32		number;
	UINT32 		begin 			= 0;

	/*
	 * get the inode corresponding to the inode number from fs in inodeBuffer
	 */
	get_inode(fs, inode, &inodeBuffer);

#if DEBUG
	printf("%s : inodeBuffer.blocks = %u\n", __func__, inodeBuffer.blocks);
#endif

	for (i = 0; i < inodeBuffer.blocks; i++) 
	{
		/*
		 * Read the block from fs->disk into block.
		 */
		block_read(fs->disk, inodeBuffer.block[i], block);

		/*
		 * Why??
		 */
		entry = (EXT2_DIR_ENTRY *) block;

		retEntry->location.block = inodeBuffer.block[i];

		/*
		 * Find entry in the EXT2_DIR_ENTRY array that is block.
		 */

#if DEBUG
		printf("%s : before find_entry_at_block call name = %s\n",
				__func__, name);
		printf("%s : name[0] = %x\n", __func__, name[0]);
#endif

		result = find_entry_at_block(block, name, begin, lastEntry, 
				&number);

#if DEBUG
		printf("%s : before switch case result = %d\n",
				__func__, result);
#endif

		switch (result) 
		{
			case -2: 
				{
#if DEBUG
					printf("%s : -2 return error\n", 
							__func__);
#endif
							
					return EXT2_ERROR;
				}
			case -1: 
				{
					 continue;
				}
			case 0: 
				{
					/*
					 * copy the entrie's contents in
					 * retEntry
					 */
					memcpy(&retEntry->entry, &entry[number],
							sizeof(EXT2_DIR_ENTRY));
					retEntry->location.group = 
						GET_INODE_GROUP(
							entry[number].inode);
					retEntry->location.block = 
						inodeBuffer.block[i];
					retEntry->location.offset = number;
					retEntry->fs = fs;
					return EXT2_SUCCESS;
				}
		}
	}

#if DEBUG
	printf("%s : exhausted all blocks in inodeBuffer, returning error\n",
			__func__);
#endif


	return EXT2_ERROR;
}

/**
 * find entry at block
 * locates the new entry position that is empty in block array.
 * @param block The block is actually an array of EXT2_DIR_ENTRY. This function 
 * 		finds a new entry space in the array.
 * @param formattedName The name that is to be searched, depending on the 
 * 			characters of the name.
 * @param begin The starting index.
 * @param last The maximum number of entries an array can hiold.
 * @param number The number that is to be returned.
 * @return Success / failure depending on operation.
 */
int 
find_entry_at_block(BYTE *block, char *formattedName, UINT32 begin, UINT32 last, 
		UINT32 *number) 
{

#if DEBUG
	printf("---------------\n");
	printf("Inside %s\n", __func__);
#endif

	UINT32 		i;
	EXT2_DIR_ENTRY 	*entry 		= (EXT2_DIR_ENTRY *) block;

	/*
	 * entry is an array of EXT2_DIR_ENTRY.
	 * entry points to first element of array.
	 * this loop returns the index of the array at which a new directory 
	 * can be added.
	 * This happens when entry->name[0] == DIR_ENTRY_FREE.
	 */
	for (i = begin; i < last; i++) 
	{

#if DEBUG
		printf("%s : formattedName[0] = %x, entry->name = %s, "
				"strlen(entry->name) = %lu\n", 
				__func__, formattedName[0], entry->name,
				strlen(entry->name));
#endif

		if (formattedName == NULL) 
		{
#if DEBUG

			printf("formattedName = NULL\n");

			printf("%s : formattedName = NULL, entry->name %s\n",
					__func__, entry->name);
#endif

			if 	((entry->name[0] != DIR_ENTRY_NO_MORE)	&& 
				(entry->name[0] != DIR_ENTRY_FREE) 	&& 
				(entry->name[0] != '.')) 
			{
				*number = i;
				return EXT2_SUCCESS;
			}
		} 
		else 
		{

#if DEBUG
			printf("%s : else part\n", __func__);
#endif

			if 	(memcmp(entry->name, formattedName, 
					MAX_NAME_LENGTH) == 0) 
			{
#if DEBUG
				printf("formattedName = entry->name\n");
#endif

				*number = i;
				return EXT2_SUCCESS;
			}

			if 	(((formattedName[0] == DIR_ENTRY_FREE)	|| 
				(formattedName[0] == DIR_ENTRY_NO_MORE))&&
				(formattedName[0] == entry->name[0])) 
			{

#if DEBUG
				printf("formattedName[0] == DIR_ENTRY_FREE\n");
#endif				

				*number = i;
				return EXT2_SUCCESS;
			}
		}

		if (entry->name[0] == DIR_ENTRY_NO_MORE) 
		{
			*number = i;
			return -2;
		}
		entry++;
	}
	*number = i;
	return EXT2_ERROR;
}

/**
 * get free inode number 
 * @param fs The EXT2_FILESYSTEM from which we have to find the free inode 
 * 	     number.
 * @return The free inode number UINT32
 */
UINT32 
get_free_inode_number(EXT2_FILESYSTEM *fs) 
{

#if DEBUG
	printf("---------------\n");
	printf("Inside %s\n", __func__);
#endif

	EXT2_INODE_BITMAP	i_bitmap;
	EXT2_BITSET 		inodeset;
	int 			i;
	int			j;
	unsigned char 		k = 0x80;

	/*
	 * Copy the inode_bitmap from fs->disk to i_bitmap.
	 * location is given by middle parameter.
	 * The value results to INODE_BITMAP_BASE.
	 */
	block_read(fs->disk, 
	(fs->sb.block_group_number * BLOCKS_PER_GROUP) + INODE_BITMAP_BASE, 
	&i_bitmap);

	/*
	 * Not sure what this loop does ?
	 * But returns the free inode number for directory creation.
	 */
	for 	(i = 0; 
		((i < fs->sb.inode_per_group) && 
		(i < MAX_BLOCK_SIZE / 4)); 
		i++) 
	{
		if (i_bitmap.inode_bitmap[i] != 0xff) 
		{
			for (j = 0; j < 8; j++) 
			{

#if DEBUG
				printf("i_bitmap.inode_bitmap[i] = %u\n",
						i_bitmap.inode_bitmap[i]);
#endif

				if (!(i_bitmap.inode_bitmap[i] & (k >> j))) 
				{

#if DEBUG
					printf("i = %u\n", i);
					printf("j = %u\n", j);
#endif

					inodeset.bit_num = 8 * i + j;

#if DEBUG
					printf("inodeset.bit_num = %u\n",
						inodeset.bit_num);
#endif

					inodeset.index_num = i;

#if DEBUG
					printf("inodeset.index_num = %u\n",
						inodeset.index_num);
#endif

					set_inode_bitmap(fs, &i_bitmap, 
							inodeset);
#if DEBUG
					printf("---------------------\n");
#endif

					return inodeset.bit_num + 1;
				}
			}
		}
	}
	return 0;
}

/**
 * get free block number.
 * @param fs The filesystem whose next free block number is to be returned.
 * @return 
 */
UINT32 
get_free_block_number(EXT2_FILESYSTEM *fs) 
{

#if DEBUG
	printf("---------------\n");
	printf("Inside %s\n", __func__);
#endif

	EXT2_BLOCK_BITMAP 	b_bitmap;
	EXT2_BITSET 		blockset;
	int 			i;
	int 			j;
	unsigned char 		k 		= 0x80;

	/*
	 * Middle parameter results in BLOCK_BITMAP_BASE
	 */
	block_read(fs->disk, 
	(fs->sb.block_group_number * BLOCKS_PER_GROUP) + BLOCK_BITMAP_BASE, 
	&b_bitmap);

	/*
	 * Not sure what this loop does.
	 * Returns the next free block number.
	 */
	for 	(i = 0; 
		((i < fs->sb.block_per_group)	&& 
		 (i < MAX_BLOCK_SIZE / 4)); 
		i++) 
	{
		if (b_bitmap.block_bitmap[i] != 0xff) 
		{
			for (j = 0; j < 8; j++) 
			{
				if (!(b_bitmap.block_bitmap[i] & (k >> j))) 
				{
					blockset.bit_num = 8 * i + j;
					blockset.index_num = i;
					set_block_bitmap(fs, &b_bitmap, 
							blockset);
					return blockset.bit_num;
				}
			}
		}
	}
	return 0;
}

/**
 * Get data block at inode.
 * @param fs The EXT2_FILESYSTEM.
 * @param inode The inode from which the data block number is to be extracted.
 * @param number The array index of inode.block[] corresponding to which the
 *               block number is to be extracted. The block number is present at
 *               this index in array.
 * @return The block number in integer.
 */
int get_data_block_at_inode(EXT2_FILESYSTEM *fs, INODE inode, UINT32 number) 
{

#if DEBUG
	printf("---------------\n");
	printf("Inside %s\n", __func__);
#endif

	return inode.block[number];
}


/******************************************************************************/
/*																			  */
/*								set function 								  */
/*																			  */
/******************************************************************************/

/**
 * insert entry
 * Insert new entry in parent node.
 * @param parent The parent node in which the entry is to be added.
 * @param newEntry The entry to be added.
 * @param fileType The fileType of the system.
 * @return Sucess / failure value.
 */
int 
insert_entry(EXT2_NODE *parent, EXT2_NODE *newEntry, UINT16 fileType) 
{

#if DEBUG
	printf("---------------\n");
	printf("Inside %s\n", __func__);
#endif


	EXT2_NODE 			entry;
	char 				entryName[2] 	= {0,};
	INODE 				inodeBuffer;
	INT32 				free_block_num;

	entryName[0] = DIR_ENTRY_FREE;

	/*
	 * condition checks whether there is space for new entry in parent 
	 * directory and if there is, it allocates the entry parameter with
	 * the location.
	 */
	if (
	lookup_entry(parent->fs, parent->entry.inode, entryName, &entry) == 
	EXT2_SUCCESS) 
	{
		/*
		 * Why to put newEntry->entry in entry.location?
		 */
		set_entry(parent->fs, &entry.location, &newEntry->entry);
		newEntry->location = entry.location;
	} 
	else 
	{
		/*
		 * Why ?
		 */
		entryName[0] = DIR_ENTRY_NO_MORE;
		if (lookup_entry(parent->fs, parent->entry.inode, entryName, 
			&entry) == EXT2_ERROR)
		{
			return EXT2_ERROR;
		}

		set_entry(parent->fs, &entry.location, &newEntry->entry);
		newEntry->location = entry.location;

		/*
		 * Why?
		 *
		 */
		entry.location.offset++;
		
		/*
		 * When the offset is incremented and reaches the end of
		 * maximum array size.
		 * Find a new free block in the filsystem.
		 * Allocate the new free block number as the last entry in the 
		 * inodeBuffer.block array.
		 * set the free block number as the location's block number 
		 * and set the offsret to 0
		 */
		if (entry.location.offset == 
			(MAX_BLOCK_SIZE / sizeof(EXT2_DIR_ENTRY)))
		{
			get_inode(parent->fs, parent->entry.inode, &inodeBuffer);

			free_block_num = get_free_block_number(parent->fs);

			inodeBuffer.block[inodeBuffer.blocks - 1] = 
				free_block_num;

			entry.location.block = free_block_num;

			entry.location.offset = 0;

		}

		/*
		 * set the new entry at new location
		 */
		set_entry(parent->fs, &entry.location, &entry.entry);
	}
	return EXT2_SUCCESS;
}

/**
 * insert inode table
 * insert the new inode entry in the inode table.
 * @param parent The parent whose  fs->disk has to be updated with new inode
 * @param inode_entry The new inode entry
 * @param free_inode The free inode number
 * @return The failure / success value.
 */
int 
insert_inode_table(EXT2_NODE *parent, INODE *inode_entry, int free_inode) 
{

#if DEBUG
	printf("---------------\n");
	printf("Inside %s\n", __func__);
#endif

	DISK_OPERATIONS 	*disk;

	disk 			= parent->fs->disk;

	BYTE 			block[MAX_BLOCK_SIZE];
	INODE 			*inode;

	free_inode--;

	UINT32 group_num = (free_inode / INODES_PER_GROUP);
	UINT32 block_num = (free_inode % INODES_PER_GROUP) / INODES_PER_BLOCK;
	UINT32 block_offset = (free_inode % INODES_PER_GROUP) % INODES_PER_BLOCK;

	block_read(disk, 
	(group_num * BLOCKS_PER_GROUP) + INODE_TABLE_BASE + block_num, 
	block);

	inode = (INODE *) block;
	inode = inode + block_offset;
	*inode = *inode_entry;

	block_write(disk, 
	(group_num * BLOCKS_PER_GROUP) + INODE_TABLE_BASE + block_num, 
	block);

	return EXT2_SUCCESS;
}

/**
 * set super block free block count
 * @param node The node whose superblock's free block count is to be updated.
 * @param num The number of the blocks that are to be decreased.
 * @return The Success/ failure 
 */
int 
set_sb_free_block_count(EXT2_NODE *node, int num) 
{

#if DEBUG
	printf("---------------\n");
	printf("Inside %s\n", __func__);
#endif
	int			result			= 0;
	BYTE 			block[MAX_BLOCK_SIZE];
	EXT2_SUPER_BLOCK 	*sb;
	DISK_OPERATIONS 	*disk 			= node->fs->disk;

	/*
	 * Read the super block from disk in block
	 */
	block_read(disk, SUPER_BLOCK_BASE, block);

	/*
	 * sb has the super block
	 */
	sb = (EXT2_SUPER_BLOCK *) block;

	/*
	 * decrease the free block count if possible.
	 */
	if (num > 0)
	{
		sb->free_block_count += num;
	}
	else if (sb->free_block_count >= -num) 
	{
		sb->free_block_count += num;
	} 
	else 
	{
		PRINTF("No more free_block exist\n");
		return EXT2_ERROR;
	}
	
	/*
	 * super  block is written
	 */
	result = block_write(disk, SUPER_BLOCK_BASE, block);

	return result;
}

/**
 * Set super block free inode count.
 * @param node The node whose filesystem's free inode count is to be updated.
 * @param num The number of inodes to be decremented.
 * @return The Success / failure value
 */
int 
set_sb_free_inode_count(EXT2_NODE *node, int num) 
{

#if DEBUG
	printf("---------------\n");
	printf("Inside %s\n", __func__);
#endif

	int			result			= 0;
	BYTE 			block[MAX_BLOCK_SIZE];
	EXT2_SUPER_BLOCK 	*sb;
	DISK_OPERATIONS 	*disk 			= node->fs->disk;

	/*
	 * Read the super block from disk in block
	 */
	block_read(disk, SUPER_BLOCK_BASE, block);

	/*
	 * sb has the super block
	 */
	sb = (EXT2_SUPER_BLOCK *) block;

	/*
	 * decrease the free inode count if possible.
	 */
	if (num > 0)
	{
		sb->free_inode_count += num;
	}
	else if (sb->free_inode_count >= -num) 
	{
		sb->free_inode_count += num;
	} 
	else 
	{
		PRINTF("No more free_inode exist\n");
		return EXT2_ERROR;
	}

	/*
	 * super  block is written
	 */
	result = block_write(disk, SUPER_BLOCK_BASE, block);

	return result;
}

/**
 * set gd free block count
 * @param node The node whose filsystem's disk's gd is to be updated.
 * @param free_data_block The free data block number in the disk.
 * @param num The number of blocks to be decremented.
 * @return Success / failure of operation
 */
int 
set_gd_free_block_count(EXT2_NODE *node, UINT32 free_data_block, int num) 
{

#if DEBUG
	printf("---------------\n");
	printf("Inside %s\n", __func__);
#endif

	int			result			= 0;
	BYTE 			block[MAX_BLOCK_SIZE];
	EXT2_GROUP_DESCRIPTOR 	*gd;
	DISK_OPERATIONS 	*disk 			= node->fs->disk;

	UINT32 			group_num 		= 
		GET_DATA_GROUP(free_data_block);
	UINT32 			gd_block_num 		= 
		group_num * sizeof(EXT2_GROUP_DESCRIPTOR) / MAX_BLOCK_SIZE;
	UINT32 			gd_block_offset 	= 
		group_num % (MAX_BLOCK_SIZE / sizeof(EXT2_GROUP_DESCRIPTOR));

	/*
	 * Read the required group descriptor from gdt.
	 */
	block_read(disk, GDT_BASE + gd_block_num, block);

	gd = (EXT2_GROUP_DESCRIPTOR *) block;

	/*
	 * Why?
	 */
	gd += gd_block_offset;

	/*
	 * decrement the free block count of gd
	 */
	if (num > 0)
	{
		gd->free_blocks_count += num;
	}
	else if (gd->free_blocks_count >= -num) 
	{
		gd->free_blocks_count += num;
	} 
	else 
	{
		PRINTF("No more free_block exist\n");
		return EXT2_ERROR;
	}

	/*
	 * Write the updated group descriptor to disk.
	 */
	result = block_write(disk, GDT_BASE + gd_block_num, block);

	return result;
}

/**
 * set gd free inode count
 * @param node The node whose filsystem's disk's gd is to be updated.
 * @param free_data_block The free inode number in the disk.
 * @param num The number of inodes to be decremented.
 * @return Success / failure of operation
 */
int 
set_gd_free_inode_count(EXT2_NODE *node, UINT32 free_inode, int num) 
{

#if DEBUG
	printf("---------------\n");
	printf("Inside %s\n", __func__);
#endif

	int			result			= 0;
	BYTE 			block[MAX_BLOCK_SIZE];
	EXT2_GROUP_DESCRIPTOR 	*gd;
	DISK_OPERATIONS 	*disk 			= node->fs->disk;

	UINT32 group_num 				= 
		GET_INODE_GROUP(free_inode);
	UINT32 			gd_block_num 		= 
		group_num * sizeof(EXT2_GROUP_DESCRIPTOR) / MAX_BLOCK_SIZE;
	UINT32 			gd_block_offset 	= 
		group_num % (MAX_BLOCK_SIZE / sizeof(EXT2_GROUP_DESCRIPTOR));

	block_read(disk, GDT_BASE + gd_block_num, block);

	gd = (EXT2_GROUP_DESCRIPTOR *) block;

	gd += gd_block_offset;

	/*
	 * decrement the free inode count of gd
	 */
	if (num > 0)
	{
		gd->free_inodes_count += num;
	}
	else if (gd->free_blocks_count >= -num)
	{
		gd->free_inodes_count += num;
	}
	else 
	{
		PRINTF("No more free_block exist\n");
		return EXT2_ERROR;
	}

	/*
 	 * Write the updated group descriptor to disk.
 	 */
	result = block_write(disk, GDT_BASE + gd_block_num, block);

	return result;
}

/**
 * set entry.
 * Set the EXT2_DIR_ENTRY at location->offset to given value.
 * @param fs The filesystem from which we have to take the EXT2_DIR_ENTRY array 
 * 	     from position given by location->block.
 * @param location The EXT2_DIR_ENTRY_LOCATION strcuture that specifies the 
 * 		   block number and the offset where the new entry is to be 
 * 		   substituted.
 * @param value The new EXT2_DIR_ENTRY value that is to be put.
 * @return The success / failure of operation.
 */
int 
set_entry(EXT2_FILESYSTEM *fs, EXT2_DIR_ENTRY_LOCATION *location, 
		EXT2_DIR_ENTRY *value) 
{

#if DEBUG
	printf("---------------\n");
	printf("Inside %s\n", __func__);
#endif

	BYTE 		block[MAX_BLOCK_SIZE];
	EXT2_DIR_ENTRY 	*entry;
	int 		result;

	/*
	 * Read the block of data at location->block into block.
	 */
	result = block_read(fs->disk, location->block, block);
	
	/*
	 * Typecast the block into array of EXT2_DIR_ENTRY
	 */
	entry = (EXT2_DIR_ENTRY *) block;

	/*
	 * set the new EXT2_DIR_ENTRY at location->offset index in entry array.
	 */
	entry[location->offset] = *value;

	/*
	 * Write the block again at the desired place.
	 */
	result = block_write(fs->disk, location->block, block);

	return result;
}

/**
 * set inode bitmap.
 * The inode bitmap 
 * @param fs The EXT2 filesystem  on whose disk the inode bitmap has to be 
 * 	     updated.
 * @param i_bitmap The inode bitmap which is to be updated.
 * @param bitset This structure specifies the index and the bit number that is '
 * 		  to be updated.
 * @return Success / Failure value.
 */
int 
set_inode_bitmap(EXT2_FILESYSTEM *fs, EXT2_INODE_BITMAP *i_bitmap, 
		EXT2_BITSET bitset) 
{

#if DEBUG
	printf("---------------\n");
	printf("Inside %s\n", __func__);
#endif

	int		result		= 0;
	DISK_OPERATIONS *disk 		= fs->disk;
	
	/*
	 * This just sets the new bit number in the index of the inode bitmap.
	 */
#if DEBUG
	printf("bitset.index_num = %u\n", bitset.index_num);
	printf("bitset.bit_num = %u\n", bitset.bit_num);
	printf("0x80 >> bitset.bit_num = %u\n", (0x80 >> bitset.bit_num));
	printf("0x80 >> (bitset.bit_num % 8) = %u\n", 
			(0x80 >> (bitset.bit_num % 8)));
	printf("i_bitmap->inode_bitmap[bitset.index_num] = %u\n", 
			i_bitmap->inode_bitmap[bitset.index_num]);
#endif

	i_bitmap->inode_bitmap[bitset.index_num] ^= 
		(0x80 >> ((bitset.bit_num % 8)));

#if DEBUG
	printf("i_bitmap->inode_bitmap[bitset.index_num] = %u\n", 
			i_bitmap->inode_bitmap[bitset.index_num]);
#endif

	/*
	 * Write the updated inode bitmap to the disk.
	 */
	result = block_write(disk, 
	(fs->sb.block_group_number * BLOCKS_PER_GROUP) + INODE_BITMAP_BASE, 
	i_bitmap);

	return result;


}

/**
 * set block bitmap
 * @param fs The filesystem on disk of which we have to update the block bitmap.
 * @parm b_bitmap The block bitmap that is to be updated.
 * @param bitset The index and the bit position that is to be set.
 * @return The Success / failure of operation.
 */
int 
set_block_bitmap(EXT2_FILESYSTEM *fs, EXT2_BLOCK_BITMAP *b_bitmap, 
		EXT2_BITSET bitset) 
{

#if DEBUG
	printf("---------------\n");
	printf("Inside %s\n", __func__);
#endif
	
	int		result	= 0;
	DISK_OPERATIONS *disk 	= fs->disk;

	/*
	 * update the block bitmap for the new block
	 */
	b_bitmap->block_bitmap[bitset.index_num] ^= 
		(0x80 >> ((bitset.bit_num % 8)));

	/*
	 * Write the new block bitmap to the disk.
	 */
	result = block_write(disk, 
	(fs->sb.block_group_number * BLOCKS_PER_GROUP) + BLOCK_BITMAP_BASE, 
	b_bitmap);

	return result;

}

/**
 * fill data block
 * fill the data block on the disk at appropriate location with buffer.
 * @param disk The disk in which the block is to be written
 * @param inodetemp The inode that holds the block number
 * @param length The length of the file
 * @param offset The offset from which we have to start writing
 * @param buffer The data to be written
 * @param block_num The block number.
 * @return
 */
void
fill_data_block(DISK_OPERATIONS *disk, INODE *inodetemp, unsigned long length, 
		unsigned long offset, const char *buffer, UINT32 block_num) 
{

#if DEBUG
	printf("---------------\n");
	printf("Inside %s\n", __func__);
#endif

	BYTE 		block[MAX_BLOCK_SIZE];
	UINT32 		copy_num = MAX_BLOCK_SIZE / SHELL_BUFFER_SIZE;
	int 		i;
	int		j;

#if DEBUG
	int		k;
#endif

	unsigned long 	lesslength = length;
	unsigned long 	cur_offset = offset;


	if (block_num != 0) 
	{
		for (i = 0; i < block_num + 1; i++) 
		{
			ZeroMemory(block, sizeof(block));
			cur_offset = 0;
			for (j = 0; j < copy_num; j++) 
			{
				memcpy(&block[cur_offset], buffer, 
						SHELL_BUFFER_SIZE);

#if DEBUG
				printf("buffer = ");
				for (k = 0; k < SHELL_BUFFER_SIZE; k++)
				{
					printf("%c", buffer[k]);
				}
				printf("\n");

				printf("block_num != 0, block[cur_offset] = ");
				for (k = 0; k < SHELL_BUFFER_SIZE; k++)
				{
					printf("%c", block[k]);
				}
				printf("\n");
#endif

				cur_offset += SHELL_BUFFER_SIZE;
			}
			block_write(disk, inodetemp->block[i], block);
			lesslength -= MAX_BLOCK_SIZE;
		}
	} 
	else 
	{
		ZeroMemory(block, sizeof(block));
		cur_offset = 0;
		for 	(i = 0; 
			(i < ((lesslength + SHELL_BUFFER_SIZE - 1) / 
				SHELL_BUFFER_SIZE)); 
			i++) 
		{
			memcpy(&block[cur_offset], buffer, SHELL_BUFFER_SIZE);

#if DEBUG
			printf("buffer = ");
			for (k = 0; k < SHELL_BUFFER_SIZE; k++)
			{
				printf("%c", buffer[k]);
			}
			printf("\n");

			printf("block_num == 0, block[cur_offset] = ");
			for (k = 0; k < SHELL_BUFFER_SIZE; k++)
			{
				printf("%c", block[k]);
			}
			printf("\n");
#endif

			cur_offset += SHELL_BUFFER_SIZE;
		}
			
#if DEBUG
		printf("inodetemp->size = %u\n",inodetemp->size);
		printf("inodetemp->blocks = %u\n", inodetemp->blocks);
		printf("block_num = %u\n", block_num);
		printf("inodetemp->block[block_num] = %u\n",
				inodetemp->block[block_num]);
#endif

		block_write(disk, inodetemp->block[block_num], block);
	}

}


/******************************************************************************/
/*																			  */
/*							utility function 								  */
/*																			  */
/******************************************************************************/

/**
 * format name
 * Convert the name to upper case and add "." after 8 characters.
 * @param name The name in question.
 * @return The Success / fail value of operation.
 */
int 
format_name(char *name) 
{

#if DEBUG
	printf("---------------\n");
	printf("Inside %s\n", __func__);
#endif

	UINT32 	i;
	UINT32	length;
	UINT32	nameLength			= 0;
	char 	regularName[MAX_NAME_LENGTH] 	= {'\0'};

	/*
	 * fill the regularName with spaces.
	 */
	memset(regularName, 0x20, sizeof(regularName));
	length = strlen(name);

	/*
	 * If the name in question is the parent directory indicator,
	 * apppend spaces to it to make its length 11 and return.
	 */
	if (strncmp(name, "..", 2) == 0) 
	{
		regularName[0] = '.';
		regularName[1] = '.';
		memcpy(name, regularName, MAX_NAME_LENGTH);
		return EXT2_SUCCESS;
	}

	/*
	 * If the name in question is current directory indicator,
	 * append spaces to it to make its length 11 and return.
	 */
	else if (strncmp(name, ".", 1) == 0) 
	{
		regularName[0] = '.';
		memcpy(name, regularName, MAX_NAME_LENGTH);
		return EXT2_SUCCESS;
	} 
	else 
	{
		for (i = 0; i < length; i++) 
		{
			/*
			 * Check for a valid name.
			 */
			if (	(name[i] != ' ') 	&& 
				(!isdigit(name[i])) 	&& 
				(!isalpha(name[i]))	&&
				(name[i] != '_'))
			{

#if DEBUG
				printf("returned error due to invalid name\n");
#endif

				return EXT2_ERROR;
			} 
			else if (	(isdigit(name[i]))	|| 
					(isalpha(name[i]))	||
					(name[i] == ' ')	||
					(name[i] == '_'))
			{
				regularName[nameLength++] = name[i];
			} 
			else
			{
				
#if DEBUG
				printf("returned Error\n");
#endif

				return EXT2_ERROR;
			}
		}

		if (	(nameLength == 0)	||
			(nameLength > MAX_NAME_LENGTH))
		{

#if DEBUG
			printf("namelength = %d\n", nameLength);
			printf("returned error due to nameLength = 0\n");
#endif

			return EXT2_ERROR;
		}
	}
	
	memcpy(name, regularName, sizeof(regularName));	

#if DEBUG
	printf("name = %s\n", name);
#endif

	return EXT2_SUCCESS;
}

/**
 * block_write.
 * This will write the given data in this.
 * The block number is given in block.
 * The block is written in chunks of sector.
 * @param this The DISK_OPERATIONS structure in the pdata(i.e.DISK_MEMORY) part 
 * 	       of which we have to write the data.
 * @param block The block number.
 * @param data The starting address of the data that is to be written.
 */
int block_write(DISK_OPERATIONS *this, SECTOR block, void *data) 
{

#if DEBUG
	printf("---------------\n");
	printf("Inside %s\n", __func__);
#endif

	int i;
	int result;

	/*
	 * This is the block number converted to sector number.
	 * Since we have to write after this sector number. 
	 */
#if DEBUG
	printf("write block number = %u\n", block);
#endif
	int sectorNum = block * SECTORS_PER_BLOCK;

	/*
	 * This loop repeats SECTORS_PER_BLOCK times.
	 * Each iteration is to write MAX_SECTOR_SIZE bytes from data to this.
	 * The sector number in which bytes are to be written is given by 
	 * sectorNum + i.
	 * The starting address of data is incremented each time, so as to 
	 * write the
	 * entire data when the loop is completed.
	 */
	for (i = 0; i < SECTORS_PER_BLOCK; i++) 
	{

#if DEBUG
		printf("write sectorNum + i = %u\n", sectorNum + i);
		printf("pdata = %lu\n", ((DISK_MEMORY *) this->pdata));

#endif
		result = disksim_write(this, sectorNum + i, 
				data + (i * MAX_SECTOR_SIZE));
		if (result != 0)
		{

#if DEBUG
			printf("block write has result = %d\n", result);
#endif
		}
	}
	return result;
}

/**
 * block_read.
 * This will read the bytes from this to the location given by data.
 * The block number is given in block.
 * The block is read in chunks of sector.
 * @param this The DISK_OPERATIONS structure from the pdata(i.e.DISK_MEMORY) part
 *	       of which we have to read the data.
 * @param block The block number.
 * @param data The starting address of the location where bytes are to be read.
 */
int block_read(DISK_OPERATIONS *this, SECTOR block, void *data) 
{

#if DEBUG
	printf("---------------\n");
	printf("Inside %s\n", __func__);
#endif

	int i;
	int result;

	/*
	 * This is the block number converted to sector number.
	 * Since we have to read after this sector number. 
	 */

#if DEBUG
	printf("read block number = %u\n", block);
#endif

	int sectorNum = block * SECTORS_PER_BLOCK;

	/*
	 * This loop repeats SECTORS_PER_BLOCK times.
	 * Each iteration is to read MAX_SECTOR_SIZE bytes from this to data.
	 * The sector number from which bytes are to be read is given by 
	 * sectorNum + i.
	 * The starting address of data is incremented each time, so as to 
	 * read the
	 * entire bytes when the loop is completed.
	 */
	for (i = 0; i < SECTORS_PER_BLOCK; i++) 
	{

#if DEBUG
		printf("read sectorNum + i = %u\n", sectorNum + i);
		printf("pdata = %lu\n", ((DISK_MEMORY *) this->pdata));
#endif

		result = disksim_read(this, sectorNum + i, 
				data + (i * MAX_SECTOR_SIZE));
		if (result != 0)
		{

#if DEBUG
			printf("block read has result = %d\n", result);
#endif

		}
	}
	return result;
}

/*
 * set_bit.
 * @param number The number that generates the byte position and offset.
 * @param bitmap The bitmap in which the bits are to be set.
 */
int 
set_bit(SECTOR number, 
	BYTE *bitmap) 
{

#if DEBUG
	printf("---------------\n");
	printf("Inside %s\n", __func__);
#endif

	BYTE value;
	SECTOR byte = number / 8;
	SECTOR offset = number % 8;

	switch (offset) 
	{
		case 0:
			value = 0x80; // 1000 0000
			break;
		case 1:
			value = 0x40; // 0100 0000
			break;
		case 2:
			value = 0x20; // 0010 0000
			break;
		case 3:
			value = 0x10; // 0001 0000
			break;
		case 4:
			value = 0x8; // 0000 1000
			break;
		case 5:
			value = 0x4; // 0000 0100
			break;
		case 6:
			value = 0x2; // 0000 0010
			break;
		case 7:
			value = 0x1; // 0000 0001
			break;
	}
	bitmap[byte] |= value;

	return EXT2_SUCCESS;
}

/**
 * zero bit set.
 * @param number
 * @param bitmap
 */
int 
set_zero_bit(SECTOR number, BYTE *bitmap) 
{

#if DEBUG
	printf("---------------\n");
	printf("Inside %s\n", __func__);
#endif

	BYTE value;
	SECTOR byte = number / 8;
	SECTOR offset = number % 8;

	switch (offset) 
	{
		case 0:
			value = 0x80; // 1000 0000
			break;
		case 1:
			value = 0x40; // 0100 0000
			break;
		case 2:
			value = 0x20; // 0010 0000
			break;
		case 3:
			value = 0x10; // 0001 0000
			break;
		case 4:
			value = 0x8; // 0000 1000
			break;
		case 5:
			value = 0x4; // 0000 0100
			break;
		case 6:
			value = 0x2; // 0000 0010
			break;
		case 7:
			value = 0x1; // 0000 0001
			break;
	}
	bitmap[byte] ^= value;

	return EXT2_SUCCESS;
}

/**
 * dump memory
 * @param disk
 * @param block
 * @return
 */
int 
dump_memory(DISK_OPERATIONS *disk, int block) 
{

#if DEBUG
	printf("---------------\n");
	printf("Inside %s\n", __func__);
#endif


	BYTE dump[1024];
	ZeroMemory(dump, sizeof(dump));
	block_read(disk, block, dump);
	int i;
	for (i = 0; i < 64; i++) 
	{
		int j;
		printf("%p\t ", &dump[i * 16]);
		for (j = (16 * i); j < (16 * i) + 16; j++) 
		{
			printf("%02x  ", dump[j]);
		}
		printf("\n");
	}
	return EXT2_SUCCESS;
}

/**
 * return string length until a space comes.
 * @param src string
 * @return length
 */
int 
my_strnlen(char *src) 
{

#if DEBUG
	printf("---------------\n");
	printf("Inside %s\n", __func__);
#endif

	int num = 0;
	while (	(*src)	&& 
		(*src != 0x20))
	{
		src++;
		num++;
	}
	return num;
}

/**
 * convert str of given length to upper case.
 * @param str The string in question.
 * @length The max length to be made uppercase. 
 */
void 
upper_string(char *str, int length) 
{

#if DEBUG
	printf("---------------\n");
	printf("Inside %s\n", __func__);
#endif

	while (*str && length-- > 0) 
	{
		*str = toupper(*str);
		str++;
	}
}

/**
 * Validate the block whether it is a superblock or not.
 * The magic signature is checked for verification.
 * @param block The block that is to be validated.
 * @return EXT2_SUCCESS on sucess, EXT2_ERROR otherwise.
 */
int
validate_sb(void *block) 
{

#if DEBUG
	printf("---------------\n");
	printf("Inside %s\n", __func__);
#endif

	EXT2_SUPER_BLOCK *sb = (EXT2_SUPER_BLOCK *) block;

	if (!(sb->magic_signature == 0xEF53))
	{
		return EXT2_ERROR;
	}

	return EXT2_SUCCESS;
}
