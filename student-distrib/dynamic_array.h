#ifndef _DYNAMIC_ARRAY_H
#define _DYNAMIC_ARRAY_H

/*** Dynamic arrays use memory more inefficiently than lists but have O(1) access time.
 *** For small arrays, use linked lists (list.h), since the overhead of allocating and freeing
 ***  is not worth the benefit of slightly faster access times (O(n) ~= O(1) for small n). ***/

// The amount by which the dynamic array resizes automatically
// The code is written under the assumption that this is an integer, and since 1 is too small,
//  and any larger amount would be inefficient, 2 is the best choice. This should not be changed
#define DYN_ARR_RESIZE_FACTOR 2

// Defines a generic dynamic array struct given the TYPE of the data stored,
//  and the name of the resulting dynamic array type (TYPE_NAME)
#define DYNAMIC_ARRAY(TYPE, TYPE_NAME) struct TYPE_NAME { \
	uint32_t capacity;                                    \
	uint32_t length;                                      \
	TYPE *data;                                           \
}

/*
 * Initializes the provided dynamic array
 *
 * INPUTS: TYPE: the type of the data that this dynamic array contains
 *         dyn_arr: a dynamic array to initialize
 */
#define DYN_ARR_INIT(TYPE, dyn_arr) ({ \
	dyn_arr.capacity = 1; \
	dyn_arr.length = 0; \
	dyn_arr.data = kmalloc(sizeof(TYPE)); \
})

/*
 * Frees the memory allocated for the provided dynamic array
 *
 * INPUTS: dyn_arr: a dynamic array that has had DYN_ARR_INIT called on it
 */
#define DYN_ARR_DELETE(dyn_arr) kfree((dyn_arr).data)

/*
 * Adds an element to the end of the array, resizing the memory if necessary
 *
 * INPUTS: TYPE: the type of the data that this dynamic array contains
 *         dyn_arr: the dynamic array to add a piece of data to
 *         new_element: the data to add to the end of the given dynamic array
 * OUTPUTS: the index of the newly added data
 */
#define DYN_ARR_PUSH(TYPE, dyn_arr, new_element) ({ \
	if ((dyn_arr).length == (dyn_arr).capacity) { \
		TYPE *new_data = kmalloc((DYN_ARR_RESIZE_FACTOR * (dyn_arr).capacity) * sizeof(TYPE)); \
		/* Copy over the old data */ \
		int i; \
		for (i = 0; i < (dyn_arr).length; i++) \
			new_data[i] = (dyn_arr).data[i]; \
		/* Free the old memory and set data to new_data */ \
		kfree((dyn_arr).data); \
		(dyn_arr).data = new_data; \
		/* Update the capacity */ \
		(dyn_arr).capacity = DYN_ARR_RESIZE_FACTOR * (dyn_arr).capacity; \
	} \
	/* Insert the new element at the end of the data */ \
	(dyn_arr).data[(dyn_arr).length] = (new_element); \
	/* Return the index where the new element was added while also incrementing it */ \
	(dyn_arr).length++; \
})

/*
 * Removes the last element of the array, resizing the memory if convenient
 * This function does not check if the array is already empty
 *
 * INPUTS: TYPE: the type of the data that this dynamic array contains
 *         dyn_arr: the array from whch to remove the last element
 */
#define DYN_ARR_POP(TYPE, dyn_arr) ({ \
	(dyn_arr).length -= 1; \
	if ((dyn_arr).length * DYN_ARR_RESIZE_FACTOR < (dyn_arr).capacity) { \
		/* Resize the array to have a capacity of dyn_arr.length + 1 */ \
		(dyn_arr).capacity = (dyn_arr).length + 1; \
		TYPE *new_data = kmalloc((dyn_arr).capacity * sizeof(TYPE)); \
		/* Copy over the old data */ \
		int i; \
		for (i = 0; i < (dyn_arr).length; i++) \
			new_data[i] = (dyn_arr).data[i]; \
		/* Free the old data and set data to new_data */ \
		kfree((dyn_arr).data); \
		(dyn_arr).data = new_data; \
	} \
})

/*
 * Removes the element at the specified index, resizing the memory if convenient
 * This function does not check that the index is valid
 *
 * INPUTS: TYPE: the type of the data that the array contains
 *         dyn_arr: the array from which to remove the element
 *         index: the index of the element to remove
 */
#define DYN_ARR_REMOVE(TYPE, dyn_arr, index) ({ \
	/* Move over all entries above the index */ \
	int i; \
	for (i = index; i < (dyn_arr).length - 1; i++) \
		(dyn_arr).data[i] = (dyn_arr).data[i + 1]; \
	/* Pop off the last element */ \
	DYN_ARR_POP(TYPE, dyn_arr); \
})

#endif
