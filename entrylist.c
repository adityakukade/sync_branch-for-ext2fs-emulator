#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <string.h>
#include "types.h"
#include "common.h"
#include "disk.h"
#include "disksim.h"
#include "shell.h"

#ifndef NULL
#define NULL	( ( void* )0 )
#endif

/**
 * Initialize the entry list.
 * Fille the entry list memory space with 0.
 * @param list The SHELL_ENTRY_LIST that is to be initialized.
 * @return 0
 */
int
init_entry_list(SHELL_ENTRY_LIST *list) 
{
	memset(list, 0, sizeof(SHELL_ENTRY_LIST));
	return 0;
}

/**
 * add entry to list
 * @param list The SHELL_ENTRY_LIST.
 * @param entry The SHELL_ENTRY that is to be added to the list.
 * @return 0
 */
int 
add_entry_list(SHELL_ENTRY_LIST *list, SHELL_ENTRY *entry) 
{
	SHELL_ENTRY_LIST_ITEM *newItem;

	newItem = (SHELL_ENTRY_LIST_ITEM *) malloc(sizeof(SHELL_ENTRY_LIST_ITEM));
	newItem->entry = *entry;
	newItem->next = NULL;

	if (list->count == 0)
	{
		list->first = list->last = newItem;
	}
	else 
	{
		list->last->next = newItem;
		list->last = newItem;
	}

	list->count++;

	return 0;
}

/**
 * Release entry list.
 * Free all nodes present in the linked list.
 * @param list The SHELL_ENTRY_LIST that is to be freed.
 */
void 
release_entry_list(SHELL_ENTRY_LIST *list) 
{
	SHELL_ENTRY_LIST_ITEM *currentItem;
	SHELL_ENTRY_LIST_ITEM *nextItem;

	if (list->count == 0)
	{
		return;
	}
	nextItem = list->first;

	do 
	{
		currentItem = nextItem;
		nextItem = currentItem->next;
		free(currentItem);
	} while (nextItem);

	list->count = 0;
	list->first = NULL;
	list->last = NULL;
}

