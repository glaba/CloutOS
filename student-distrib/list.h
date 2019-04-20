#ifndef _LIST_H
#define _LIST_H

// Defines a struct that represents a linked list node containing an item of type TYPE
#define LIST_ITEM(TYPE, TYPE_NAME) struct TYPE_NAME { \
	struct TYPE_NAME *next;                           \
	TYPE data;                                        \
}

// Defines a struct that represents a linked list node containing an item of type TYPE
//  as well as a unique ID for each node in the linked list
#define LIST_ITEM_ID(TYPE, TYPE_NAME) struct TYPE_NAME { \
	struct TYPE_NAME *next;                              \
	unsigned int id;                                     \
	TYPE data;                                           \
}

// Defines a struct that represents a linked list node containing an pointer to an item of type TYPE
//  as well as a unique ID for each node in the linked list
#define LIST_ITEM_ID_PTR(TYPE, TYPE_NAME) struct TYPE_NAME { \
	struct TYPE_NAME *next;                                  \
	unsigned int id;                                         \
	TYPE *data;                                              \
}

/*
 * Insert the new item into the linked list with a unique ID
 * This can be done in linear time beacuse we will sort the list by ID
 *
 * INPUTS: TYPE_NAME: the type name of the list item
 *         list_head: a pointer to the head of the list with type LIST_ITEM_ID(TYPE)
 *         new_item: a pointer to the new item to be added to the list
 * SIDE EFFECTS: the ID field of the new item is set to its new ID
 */
#define INSERT_WITH_UNIQUE_ID(TYPE_NAME, list_head, new_item) ({   \
	/* Find an ID that is not used in the list */                  \
	unsigned int id;                                               \
	TYPE_NAME *cur, *prev;                                         \
	for (prev = NULL, cur = list_head, id = 1;                     \
		 cur != NULL;                                              \
		 prev = cur, cur = cur->next, id++) {                      \
                                                                   \
		if (cur->id != id)                                         \
			break;                                                 \
	}                                                              \
                                                                   \
	/* Insert the new element into the list with the IDs */        \
	/*  in sorted order */                                         \
	new_item->id = id;                                             \
	if (prev == NULL) {                                            \
		new_item->next = list_head;                                \
		list_head = new_item;                                      \
	} else {                                                       \
		new_item->next = cur;                                      \
		prev->next = new_item;                                     \
	}                                                              \
	id;                                                            \
})

#endif
