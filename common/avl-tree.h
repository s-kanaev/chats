#ifndef _CHATS_COMMON_LIB_AVL_TREE_H_
# define _CHATS_COMMON_LIB_AVL_TREE_H_

# include <stdbool.h>
# include <stddef.h>

struct avl_tree;
typedef struct avl_tree avl_tree_t;

avl_tree_t *avl_tree_init(size_t data_size);
void avl_tree_deinit(avl_tree_t *avl_tree);
void *avl_tree_get(avl_tree_t *avl_tree, long value);
void *avl_tree_add(avl_tree_t *avl_tree, long value);
bool avl_tree_remove_by_value(avl_tree_t *avl_tree, long value);
bool avl_tree_remove_by_data(avl_tree_t *avl_tree, void *data);
/*void *avl_tree_add(avl_tree_t *avl_tree, int weight);*/

#endif /* _CHATS_COMMON_LIB_AVL_TREE_H_ */
