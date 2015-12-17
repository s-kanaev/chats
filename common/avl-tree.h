#ifndef _CHATS_COMMON_LIB_AVL_TREE_H_
# define _CHATS_COMMON_LIB_AVL_TREE_H_

# include <stdbool.h>
# include <stddef.h>

struct avl_tree;
typedef struct avl_tree avl_tree_t;

struct avl_tree_node;
typedef struct avl_tree_node avl_tree_node_t;

struct avl_tree_node {
    avl_tree_t *host;
    avl_tree_node_t *left;                                  ///< less-than value
    avl_tree_node_t *right;                                 ///< more-than value
    avl_tree_node_t *parent;
    unsigned char height;                                   ///< balance_factor = height(left) - height(right)
    long long key;
    void *data;
};

struct avl_tree {
    avl_tree_node_t *root;
};

avl_tree_t *avl_tree_init();
void avl_tree_deinit(avl_tree_t *avl_tree, bool deallocate_data);
avl_tree_node_t *avl_tree_get(avl_tree_t *avl_tree, long long int key);
avl_tree_node_t *avl_tree_add(avl_tree_t *avl_tree, long long key, void *data);
void *avl_tree_remove(avl_tree_t *avl_tree, long long key);
avl_tree_node_t *avl_tree_next(avl_tree_node_t *node);
avl_tree_node_t *avl_tree_prev(avl_tree_node_t *node);
avl_tree_node_t *avl_tree_min(avl_tree_node_t *root);
avl_tree_node_t *avl_tree_max(avl_tree_node_t *root);

#endif /* _CHATS_COMMON_LIB_AVL_TREE_H_ */
