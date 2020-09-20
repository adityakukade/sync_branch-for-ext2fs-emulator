#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <unistd.h>
#include "types.h"
#include "common.h"
#include "disk.h"
#include "disksim.h"
#include "ext2.h"

typedef struct 
{
	char *address;
} DISK_MEMORY;

/**
 * disk sim initialize
 * @param numberOfSectors
 * @param bytesPerSector
 * @param disk
 * @return
 */

int 
disksim_init(SECTOR numberOfSectors, unsigned int bytesPerSector, 
		DISK_OPERATIONS *disk) 
{
	if (disk == NULL)
	{
		return -1;
	}

	disk->pdata = malloc(sizeof(DISK_MEMORY));
	if (disk->pdata == NULL) 
	{
		disksim_uninit(disk);
		return -1;
	}

	((DISK_MEMORY *) disk->pdata)->address = (char *) malloc(
		bytesPerSector * numberOfSectors);

	if (disk->pdata == NULL) 
	{
		disksim_uninit(disk);
		return -1;
	}

	memset(((DISK_MEMORY *) disk->pdata)->address, 0, 
			bytesPerSector * (numberOfSectors));

	disk->read_sector = disksim_read;
	disk->write_sector = disksim_write;
	disk->numberOfSectors = numberOfSectors;
	disk->bytesPerSector = bytesPerSector;

	return 0;
}

/**
 * disk sim uninitialize 
 * @param this
 */
void 
disksim_uninit(DISK_OPERATIONS *this) 
{
	if (this) 
	{
		if (this->pdata)
		{
			free(this->pdata);
		}
	}
}

/**
 * disksim_read.
 * This will read this->bytesPerSector number of bytes 
 * from disk[sector * this->bytesPerSector] to the address given by data.
 * @param this The DISK_OPERATIONS structure from which the bytes are to be read.
 * 	       The bytes are read from the DISK_MEMORY part of this.
 * 	       The bytes are read sector wise.
 * @param sector The sector num of the disk from which the bytes are to be read.
 * 		 It helps to calculate the starting address from where the bytes
 * 		 are to be read from the disk.
 * @param data The starting address of the location where bytes are to be read.
 * 	       At most bytesPerSector number of bytes are read in data.
 */
int 
disksim_read(DISK_OPERATIONS *this, SECTOR sector, void *data) 
{

#if DEBUG
	printf("-------------------\n");
	printf("inside %s\n", __func__);

	if (((DISK_MEMORY *) this->pdata) == NULL)
	{
		printf("NULL memory access\n");
	}
#endif

	char *disk = (char *) ((DISK_MEMORY *) this->pdata)->address;

#if DEBUG
	printf("pdata = %lu\n", ((DISK_MEMORY *) this->pdata));
#endif

	if (sector < 0 || sector >= NUMBER_OF_SECTORS)
	{
		return -1;
	}
	/*
	 * The below statement does the copying.
	 * [sector * this->bytesPerSector] will ensure that the starting address of 
	 * the sector of the disk from which the bytes are to be read is properly 
	 * incremented.
	 */
	memcpy(data, &disk[sector * this->bytesPerSector], this->bytesPerSector);

	return 0;
}

/**
 * disksim_write.
 * This will write this->bytesPerSector number of bytes 
 * from data to disk[sector * this->bytesPerSector].
 * @param this The DISK_OPERATIONS structure in which the data is to be written.
 * 	       The data is written in the DISK_MEMORY part of this.
 * 	       The data is written sector wise.
 * @param sector The sector number of the disk in which the data is to be written.
 * 		 It helps to calculate the starting address where the data is 
 * 		 to be written in the disk.
 * @param data The starting address of the data which is to be written.
 * 	       The data contains at most bytesPerSector number of bytes.
 */

int disksim_write(DISK_OPERATIONS *this, SECTOR sector, const void *data) 
{

#if DEBUG
	printf("-------------------\n");
	printf("inside %s\n", __func__);
#endif

	char *disk = (char *) ((DISK_MEMORY *) this->pdata)->address;

#if DEBUG
	printf("pdata = %lu\n", ((DISK_MEMORY *) this->pdata));
#endif

	if (sector < 0 || sector >= NUMBER_OF_SECTORS)
	{
		return -1;
	}

	/*
	 * The below statement does the copying.
	 * [sector * this->bytesPerSector] will ensure that the starting address of 
	 * the sector of the disk in which the data is to be written is properly 
	 * incremented.
	 */
	memcpy(&disk[sector * this->bytesPerSector], data, this->bytesPerSector);

	return 0;
}
