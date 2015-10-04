#ifndef _COMMON_LIB_H_
#define _COMMON_LIB_H_

# include <stdlib.h>

/* common facilities (data structures and algorithms) */

/* bidirected list */
typedef struct list_entry {
    struct list_entry *prev;                                ///< = NULL if first
    struct list_entry *next;                                ///< = NULL if last
} list_entry_t;

/** Common usage:
 * \code{c}
 * struct Contatiner {
 *   list_entry_t le;
 *   ... some contained data ...
 * };
 * \endcode
 */

void *allocate(size_t size);
void deallocate(void *d);

/* list operations */
/** allocate first element of size = \ref size > sizeof(list_entry_t) */
void *list_init(size_t size);
/** add after \ref le an element of size \ref size
 * \return ptr to new element
 */
void *list_add(list_entry_t *le, size_t size);
/** remove element from list
 * \return ptr to next element
 */
void *remove_from_list(list_entry_t *le);
/** purge list from \ref head. call \ref func right before each removal */
void purge_list(list_entry_t *head, void (*func)(void *));

#endif /* _COMMON_LIB_H_ */
