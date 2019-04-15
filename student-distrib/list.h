#ifndef _LIST_H
#define _LIST_H

#define LIST_ITEM(TYPE) struct TYPE##_list_item { \
	TYPE data;                                    \
	struct TYPE##_list_item *next;                \
}

#endif
