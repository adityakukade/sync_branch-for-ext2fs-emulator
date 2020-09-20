#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <unistd.h>
#include <memory.h>
#include "types.h"
#include "common.h"
#include "disk.h"
#include "disksim.h"
#include "shell.h"
#include "ext2.h"
#include "ext2_shell.h"

typedef struct 
{
	char * address;
}DISK_MEMORY;

static SHELL_FILE_OPERATIONS g_file =
{
	fs_create,
	fs_remove,
	fs_read,
	fs_write
};

static SHELL_FS_OPERATIONS g_fsOprs =
{
	fs_read_dir,
	NULL,
	fs_mkdir,
	fs_rmdir,
	fs_lookup,
	&g_file,
	NULL
};

static SHELL_FILESYSTEM g_fat =
{
	"EXT2",
	fs_mount,
	fs_umount,
	fs_format,
	fs_sync
};
/******************************************************************************/
/*																			  */
/*								FS_COMMAND	 								  */
/*																			  */
/******************************************************************************/

/**
 * mount the filesystem.
 */
int 
fs_mount(DISK_OPERATIONS* disk, 
		SHELL_FS_OPERATIONS* fsOprs, 
		SHELL_ENTRY* root)
{
	EXT2_FILESYSTEM* fs;
	EXT2_NODE ext2_entry;
	int result;

	*fsOprs = g_fsOprs;

	/*
	 * fsOprs->pdata is converted into EXT2_FS
	 */
	fsOprs->pdata = malloc(sizeof(EXT2_FILESYSTEM));
	fs = FSOPRS_TO_EXT2FS(fsOprs);
	ZeroMemory(fs, sizeof(EXT2_FILESYSTEM));

	/*
	 * The fsOprs->pdata converted to EXT2_FS is allocated the formatted disk.
	 */
	fs->disk = disk;

	/* 
	 * read_superblock from fs in ext2_entry.
	 */	
	result = read_superblock(fs, &ext2_entry);

	if (result == EXT2_SUCCESS)
	{
		printf("number of groups         : %d\n", NUMBER_OF_GROUPS);
		printf("blocks per group         : %d\n", fs->sb.block_per_group);
		printf("bytes per block          : %d\n", disk->bytesPerSector);
		printf("free block count	 : %d\n", 
				fs->sb.free_block_count);
		printf("free inode count	 : %d\n", 
				fs->sb.free_inode_count);
		printf("first non reserved inode : %d\n", 
				fs->sb.first_non_reserved_inode);
		printf("inode structure size	 : %d\n", 
				fs->sb.inode_structure_size);
		printf("first data block number  : %d\n\n", 
				fs->sb.first_data_block_each_group);
	}

	/*
	 * Fill root elements with the appropriate information from ext2_entry.
	 */
	ext2_entry_to_shell_entry(&ext2_entry, root);

	return result;
}

/**
 * fs unmount
 * @param disk
 * @param fsOprs
 */
void 
fs_umount(DISK_OPERATIONS* disk, SHELL_FS_OPERATIONS* fsOprs)
{
	return;
}

/**
 * fs format
 * @param disk
 * @return
 */
int 
fs_format(DISK_OPERATIONS* disk)
{
	printf("formatting as a EXT2\n");
	ext2_format(disk);

	return  1;
}

/**
 * sync
 */
int
fs_sync(DISK_OPERATIONS *disk)
{
	int	result;

	printf("syncing...\n");
	result = ext2_sync(disk);

	return result;
}

/**
 * fs read
 */
int
fs_read( DISK_OPERATIONS* disk, SHELL_FS_OPERATIONS* fsOprs, 
	const SHELL_ENTRY* parent, SHELL_ENTRY* entry, unsigned long offset, 
	unsigned long length, char* buffer )
{

	int	result = -1;
	EXT2_NODE    EXT2Entry;

	result = shell_entry_to_ext2_entry( entry, &EXT2Entry );
	if (result != 0)
	{ 
		printf("read failed\n");
	}

	return ext2_read( &EXT2Entry, offset, length, buffer );
}

/**
 * filesystem write.
 * This function writes in filesystem.
 */
int 
fs_write(DISK_OPERATIONS* disk, SHELL_FS_OPERATIONS* fsOprs, 
		const SHELL_ENTRY* parent, SHELL_ENTRY* entry, 
		unsigned long offset, unsigned long length, const char* buffer)
{
	EXT2_NODE EXT2Entry;

	shell_entry_to_ext2_entry(entry, &EXT2Entry);

	return ext2_write(&EXT2Entry, offset, length, buffer);
}

/**
 * Create file call for filesystem.
 * @param disk The disk that is used for storing data.
 * @param fsOprs The file operations structure that has the allocated function
 * 		 pointers.
 * @param parent The SHELL_ENTRY in which a new file is to be created.
 * @param name The name of the file to be created.
 * @param retEntry The SHELL_ENTRY for the return entry. 
 * @return The success / failure of operations.
 */
int	
fs_create(DISK_OPERATIONS* disk, SHELL_FS_OPERATIONS* fsOprs, 
	const SHELL_ENTRY* parent, const char* name, SHELL_ENTRY* retEntry)
{
	EXT2_NODE	EXT2Parent;
	EXT2_NODE	EXT2Entry;
	int		result;

	shell_entry_to_ext2_entry(parent, &EXT2Parent);

	result = ext2_create(&EXT2Parent, name, &EXT2Entry);
	
	if (result == 0)
	{
		ext2_entry_to_shell_entry(&EXT2Entry, retEntry);
	}

	return result;
}

/**
 * mkdir in filesystem
 * @param disk The disk with the data to be inserted.
 * @param fsOprs The file operations structure.
 * @param parent The parent in which new entry is to be added.
 * @param name The name of the new directory.
 * @param retEntry The new entry.
 * @return The Success / failure of operation.
 */
int 
fs_mkdir(DISK_OPERATIONS* disk, SHELL_FS_OPERATIONS* fsOprs, 
	const SHELL_ENTRY* parent, const char* name, SHELL_ENTRY* retEntry)
{
	EXT2_NODE      EXT2_Parent;
	EXT2_NODE      EXT2_Entry;
	int               result;

	/*
	 * Check whether there is an entry of name already present in 
	 * parent directory.
	 */
	if (is_exist(disk, fsOprs, parent, name))
	{

#if DEBUG
		printf("%s : is_exist success\n", __func__);
#endif

		return EXT2_ERROR;
	}

	/*
	 * convert the parent shell entry into corresponding ext2 entry.
	 */
	result = shell_entry_to_ext2_entry(parent, &EXT2_Parent);

#if DEBUG
	printf("%s : shell_entry_to_ext2_entry result = %d\n", 
			__func__, result);
#endif

	/*
	 * EXT2_Parent is the parent directory converted to EXT2_NODE.
	 */
	result = ext2_mkdir(&EXT2_Parent, name, &EXT2_Entry);

#if DEBUG
	printf("%s : ext2_mkdir result = %d\n", 
			__func__, result);
	printf("in fs_mkdir EXT2_Entry.entry.name = %s\n", EXT2_Entry.entry.name);
#endif

	/*
	 * convert ext2 entry to shell entry
	 */
	if (result != EXT2_ERROR)
	{
		ext2_entry_to_shell_entry(&EXT2_Entry, retEntry);
	}

	return result;
}

/**
 * fs lookup
 * @param disk
 * @param fsOprs
 * @param parent
 * @param entry
 * @param name
 * @return
 */
int 
fs_lookup(DISK_OPERATIONS* disk, SHELL_FS_OPERATIONS* fsOprs, 
		const SHELL_ENTRY* parent, SHELL_ENTRY* entry, const char* name)
{
	EXT2_NODE	EXT2Parent;
	EXT2_NODE	EXT2Entry;
	int		result;

	shell_entry_to_ext2_entry(parent, &EXT2Parent);

	if ((result = (ext2_lookup(&EXT2Parent, name, &EXT2Entry))) == -1)
	{
		return result;
	}

	ext2_entry_to_shell_entry(&EXT2Entry, entry);

	return result;
}

/**
 * read directory in filesytem.
 * This basically reads the directory entries present in the parent directory
 * that are user made and adds them to the list.
 * @param disk The disk of the EXT2_NODE's filesytem. No need of  this parameter 
 * 	       though.	
 * @param fsOprs The SHELL_FS_OPERATIONS that has this mkdir function.
 * 		 No need of this parameter though. 
 * @param parent The SHELL_ENTRY whose directory entries are to be added in the
 * 		 list.
 * @param list The list in which the directory entries from parent are to be 
 * 	       added.
 * @return EXT2_SUCCESS
 */
int
fs_read_dir(DISK_OPERATIONS* disk, SHELL_FS_OPERATIONS* fsOprs, 
		const SHELL_ENTRY* parent, SHELL_ENTRY_LIST* list)
{
	EXT2_NODE   entry;

	/*
	 * Free all the nodes of the list first.
	 */
	if (list->count)
	{
		release_entry_list(list);
	}

	/*
	 * Get the EXT2_NODE information from the parent SHELL_ENTRY.
	 * Its pdata will be used.
	 * The information is stored in entry.
	 */
	shell_entry_to_ext2_entry(parent, &entry);

	/*
	 * entry now has data from parent's pdata.
	 * Read the directory entries from entry's EXT2_DIR_ENTRY into list.
	 */
	ext2_read_dir(&entry, adder, list);

	return EXT2_SUCCESS;
}

/**
 * fs rmdir.
 * @param disk The disk in which data is present.
 * @param fsOprs file opeartions structure.
 * @param parent The parent entry.
 * @param name The name that is to be removed.
 */
int 
fs_rmdir(DISK_OPERATIONS* disk, SHELL_FS_OPERATIONS* fsOprs, 
		const SHELL_ENTRY* parent, const char* name)
{
	EXT2_NODE	EXT2Parent;//insert modify
	EXT2_NODE 	dir;
	int 		result;

	shell_entry_to_ext2_entry(parent, &EXT2Parent);

#if DEBUG
	printf("%s : calling ext2_lookup with name = %s\n",
			__func__, name);
#endif

	result = ext2_lookup(&EXT2Parent, name, &dir);

#if DEBUG
	printf("fs_rmdir result = %d\n", result);
#endif

	if (result)
	{
		return EXT2_ERROR;
	}
		
	return ext2_rmdir(&EXT2Parent , &dir);
}

/**
 *
 */
int 
fs_remove(DISK_OPERATIONS* disk, SHELL_FS_OPERATIONS* fsOprs, 
		const SHELL_ENTRY* parent, const char* name)
{
	EXT2_NODE    EXTParent;
	EXT2_NODE    rmfile;
	int result;

	shell_entry_to_ext2_entry(parent, &EXTParent);
	result=ext2_lookup(&EXTParent, name, &rmfile);

	if (result)
	{
		return EXT2_ERROR;
	}

	return ext2_remove(&EXTParent, &rmfile);
}

/******************************************************************************/
/*																			  */
/*							UTILITY FUNCTIONS 								  */
/*																			  */
/******************************************************************************/

/**
 * SHELL_ENTRY->pdata to EXT2_NODE.
 * @param shell_entry The SHELL_ENTRY that is to be typecasted.
 * @fat_entry The EXT2_NODE that is to be filled.
 * @return EXT2_SUCCESS
 */
int 
shell_entry_to_ext2_entry(const SHELL_ENTRY* shell_entry, EXT2_NODE* fat_entry)
{
	EXT2_NODE* entry = (EXT2_NODE*)shell_entry->pdata;

	*fat_entry = *entry;

	return EXT2_SUCCESS;
}

/**
 * Fill shell entry information using ext2 entry.
 * @param ext2_entry The ext2 entry from which the inode is to be used to set the
 * 		     elements in shell entry.
 * @param shell_entry The shell entry whose elements are to be filled.
 * @return The Success / failure value. 
 */
int 
ext2_entry_to_shell_entry(const EXT2_NODE* ext2_entry, SHELL_ENTRY* shell_entry)
{
	/*
	 * Why is shell_entry->pdata converted to EXT2_NODE?
	 */
	EXT2_NODE* 	entry = ( EXT2_NODE* )shell_entry->pdata;
	BYTE*    	str;
	UINT32 		inodeno;

	inodeno	= ext2_entry->entry.inode;

	INODE 		ino;

	/*
	 * copy the inode corresponding to inodeno from fs into ino.
	 */
	get_inode(ext2_entry->fs, inodeno, &ino);
	memset( shell_entry, 0, sizeof( SHELL_ENTRY ) );

#if DEBUG
	printf("in ext2_entry_to_shell_entry inodeno = %d\n", inodeno);
#endif

	if(inodeno != 2)
	{
		str = shell_entry->name;
		str = my_strncpy( str, ext2_entry->entry.name, 255);
	}

	/*
	 * Check for directory
	 */
	if(((ino.mode >> 12) == (0x4000 >> 12))	|| 
			(inodeno == 2)) 
	{
		shell_entry->isDirectory = 1;
	}
	else
	{
		shell_entry->isDirectory = 0;
	}

	shell_entry->size=ino.size; 

	/*
	 * Why do we need this?
	 */
	*entry = *ext2_entry;

	return EXT2_SUCCESS;
}

/**
 * my strnicmp
 * compare two strings upto certain length.
 * This basically checks whether the directory has an entry of that name 
 * previously present or not.
 * @param str1 The first string
 * @param str2 The second string
 * @param length The length until which we have to check.
 * @return (1/-1) if not equal, 0 if equal.
 */
int 
my_strnicmp(const char* str1, const char* str2, int length)
{
	char   	c1;
	char	c2;

#if DEBUG
	printf("--------------\n");
#endif

	while (((*str1) || (*str2)) && 
			length-- > 0)
	{
		
		c1 = *str1;
		c2 = *str2;

#if DEBUG
		printf("c1 = %d, c2 = %d\n", c1, c2);
#endif

		if (c1 > c2)
		{
			return -1;
		}
		else if (c1 < c2)
		{
			return 1;
		}
		str1++;
		str2++;
	}

	return 0;
}

/*
 * is exist.
 * Check if the name is already present in the directory or not.
 * @param disk The disk that has the data.
 * @param fsOprs The SHELL_FS_OPERATIONS structure needed.
 * @param parent The directory where the new directory is to be made.
 * @param name The name of the new directory
 * @return EXT2_ERROR if name already exists, else EXT2_SUCCESS
 */
int 
is_exist(DISK_OPERATIONS* disk, SHELL_FS_OPERATIONS* fsOprs, 
		const SHELL_ENTRY* parent, const char* name)
{
	SHELL_ENTRY_LIST      		list;
	SHELL_ENTRY_LIST_ITEM*   	current;
	char 				check_name[MAX_NAME_LENGTH]	= {'\0'};

	memcpy(check_name, name, MAX_NAME_LENGTH);

	if((format_name(check_name)) == EXT2_ERROR)
	{
		return EXT2_ERROR;
	}

	/*
	 * initialize the SHELL_ENTRY_LIST list.
	 */
	init_entry_list(&list);

	/*
	 * Read the parent directory's directory entries in list.
	 */
	fs_read_dir(disk, fsOprs, parent, &list);
	current = list.first;

	/*
	 * This loop checks whether the directory has an entry of that name or 
	 * not by using list entries.
	 */
	while (current)
	{

#if DEBUG
		printf("----------------\n");
		printf("name = %s\n", name);
		printf("current->entry.name = %s\n", current->entry.name);
#endif

		if (my_strnicmp((char*)current->entry.name, check_name, 
					MAX_NAME_LENGTH) == 0)
		{

#if DEBUG
			printf("Are same\n");
#endif

			release_entry_list(&list);
			return EXT2_ERROR;
		}

		current = current->next;
	}

	/*
	 * Release the entry list every time.
	 */
	release_entry_list(&list);
	return EXT2_SUCCESS;
}

/**
 *
 */
unsigned char* 
my_strncpy(unsigned char* dest, const char* src, int length )
{
	while( (*src) 		&&  
		(length-- > 0) )
	{
		*dest++ = *src++;
	}

	return dest;
}

#if 0
/**
 *
 */
void
printFromP2P(char * start, char * end)
{
	int start_int, end_int;
	start_int = (int)start;
	end_int = (int)end;

	printf("start address : %#x , end address : %#x\n\n", start, end - 1);
	start = (char *)(start_int &= ~(0xf));
	end = (char *)(end_int |= 0xf);

	while (start <= end)
	{
		if ((start_int & 0xf) == 0)
		{
			fprintf(stdout, "\n%#08x   ", start);
		}

		fprintf(stdout, "%02X  ", *(unsigned char *)start);
		start++;
		start_int++;
	}
	printf("\n\n");
}
#endif

/**
 * Adder function.
 * Adds the EXT2_NODE entry to SHELL_ENTRY_LIST list.
 * It converts the ext2_entry to shell_entry->pdata first.
 * @param list The SHELL_ENTRY_LIST to which entry is to be added.
 * @param entry The EXT2_NODE that is to be added.
 * @return 
 */
int adder(void* list, EXT2_NODE* entry)
{
	SHELL_ENTRY_LIST*    entryList = (SHELL_ENTRY_LIST*)list;
	SHELL_ENTRY            newEntry;
	
	/*
	 * Convert EXT2_NODE entry to SHELL_ENTRY->pdata.
	 */
	ext2_entry_to_shell_entry(entry, &newEntry);

	/*
	 * Add new entry to entrylist. 
	 */
	add_entry_list(entryList, &newEntry);

	return EXT2_SUCCESS;
}

/**
 * REgister the filesystem.
 * Allocate the given file sytem pointer to the SHELL_FILESYSTEM present in the 
 * file.
 * @param fs The SHELL_FILESYSTEM who is to be registetered.
 */
void shell_register_filesystem(SHELL_FILESYSTEM* fs)
{
	*fs = g_fat;
}
