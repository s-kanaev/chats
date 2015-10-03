#ifndef _COMMON_LIB_H_
#define _COMMON_LIB_H_

# include <stdlib.h>

/* common facilities (data structures and algorithms) */

/* bidirected list */
typedef struct bidirected_list {
    struct bidirected_list *prev;
    struct bidirected_list *next;
} bidirected_list_t;

/** Common usage:
 * \code{c}
 * struct Contatiner {
 *   bidirected_list_t list;
 *   ... some contained data ...
 * };
 * \endcode
 */

/* list allocators/deallocators */
# define ALLOCATE(typename) (typename *)malloc(sizeof(typename))
# define DEALLOCATE(x) do { free(x); } while (0)
# define ADD_TO_LIST(end, typename, element) \
    do {                                     \
        typename *x = ALLOCATE(typename);    \
        x.element.prev = end;                \
        end.element.next = x;                \
    } while (0)
# define REMOVE_FROM_LIST(pos, element)                    \
    do {                                                   \
        struct bidirected_list_t *prev = pos.element.prev; \
        struct bidirected_list_t *next = pos.element.next; \
        prev.next = next;                                  \
        next.prev = prev;                                  \
        DEALLOCATE(pos);                                   \
    } while (0)

#endif /* _COMMON_LIB_H_ */
