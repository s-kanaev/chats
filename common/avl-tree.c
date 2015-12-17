#include "avl-tree.h"
#include "memory.h"

#include <stdbool.h>
#include <assert.h>

/****************** node **********************/
static
void node_purge(avl_tree_node_t *node, bool deallocate_data) {
    if (!node) return;
    node_purge(node->left, deallocate_data);
    node_purge(node->right, deallocate_data);

    if (deallocate_data && node->data) deallocate(node->data);
    deallocate(node);
}

static
avl_tree_node_t *node_init(long long key, void *data, avl_tree_t *host) {
    avl_tree_node_t *n = allocate(sizeof(avl_tree_node_t));
    assert(n);

    n->host = host;
    n->key = key;
    n->parent = n->left = n->right = NULL;
    n->height = 1;
    n->data = data;
}

static
void node_deinit(avl_tree_node_t *n) {
    deallocate(n);
}

static
unsigned char node_height(avl_tree_node_t *n) {
    return n ? n->height : 0;
}

static
int node_balance_factor(avl_tree_node_t *n) {
    return node_height(n->right) - node_height(n->left);
}

static
void node_fix_height(avl_tree_node_t *n) {
    unsigned char hl = node_height(n->left),
                  hr = node_height(n->right);
    n->height = (hl > hr ? hl : hr) + 1;
}

/****************** tree **********************/
static
avl_tree_node_t *tree_rotate_right(avl_tree_node_t *p) {
    avl_tree_node_t *q = p->left;
    avl_tree_node_t *p_parent = p->parent;

    p->left = q->right;
    q->right = p;

    if (p->left) p->left->parent = p;
    q->right->parent = q;
    q->parent = p_parent;

    node_fix_height(p);
    node_fix_height(q);

    return q;
}

static
avl_tree_node_t *tree_rotate_left(avl_tree_node_t *q) {
    avl_tree_node_t *p = q->right;
    avl_tree_node_t *q_parent;

    q->right = p->left;
    p->left = q;

    if (q->right) q->right->parent = q;
    p->left->parent = p;
    p->parent = q_parent;

    node_fix_height(q);
    node_fix_height(p);

    return p;
}

static
avl_tree_node_t *tree_balance(avl_tree_node_t *p) {
    node_fix_height(p);

    switch (node_balance_factor(p)) {
        case +2:
            if (node_balance_factor(p->right) < 0) p->right = tree_rotate_right(p->right);
            return tree_rotate_left(p);
            break;
        case -2:
            if (node_balance_factor(p->left) > 0) p->left = tree_rotate_left(p->left);
            return tree_rotate_right(p);
            break;
    }

    return p;
}

static
avl_tree_node_t *tree_insert(avl_tree_node_t *p, long long key, void *data, avl_tree_t *host) {
    int cmp_ret;

    if (!p) return node_init(key, data, host);

    if (key < p->key) p->left = tree_insert(p->left, key, data, host);
    else /*if (value > p->value)*/ p->right = tree_insert(p->right, key, data, host);

    tree_balance(p);
}

static
avl_tree_node_t *tree_remove_minimum_node(avl_tree_node_t *p) {
    if (!p->left) return p->right;
    p->left = tree_remove_minimum_node(p->left);

    return tree_balance(p);
}

static
avl_tree_node_t *tree_remove_node(avl_tree_node_t *p, long long key,
                                  void **return_data) {
    if (!p) return NULL;
    if (key < p->key) p->left = tree_remove_node(p->left, key, return_data);
    else if (key > p->key) p->right = tree_remove_node(p->right, key, return_data);
    else {
        avl_tree_node_t *q = p->left,
                        *r = p->right,
                        *p_parent = p->parent,
                        *min;
        return_data = p->data;
        node_deinit(p);

        if (!r) {
            if (q) q->parent = p_parent;
            return q;
        }

        min = avl_tree_min(r);
        min->right = tree_remove_minimum_node(r);
        min->left = q;

        if (min->left) min->left->parent = min;
        if (min->right) min->right->parent = min;
        min->parent = p_parent;

        return tree_balance(min);
    }

    return tree_balance(p);
}

static
avl_tree_node_t *tree_find(avl_tree_node_t *p, long long key) {
    if (!p) return NULL;
    if (key < p->key) return tree_find(p->left, key);
    else if (key > p->key) return tree_find(p->right, key);
    return p;
}

static
bool tree_is_left(avl_tree_node_t *p) {
    return p ? p->parent && p->parent->left == p : false;
}

static
bool tree_is_right(avl_tree_node_t *p) {
    return p ? p->parent && p->parent->right == p : false;
}

/****************** API ***********************/
avl_tree_t *avl_tree_init() {
    avl_tree_t *avl_tree = allocate(sizeof(avl_tree_t));

    if (!avl_tree) return NULL;

    avl_tree->root = NULL;

    return avl_tree;
}

void avl_tree_deinit(avl_tree_t *avl_tree, bool deallocate_data) {
    if (!avl_tree) return;

    node_purge(avl_tree->root, deallocate_data);
    deallocate(avl_tree);
}

avl_tree_node_t *avl_tree_add(avl_tree_t *avl_tree, long long int key, void *data) {
    avl_tree->root = tree_insert(avl_tree->root, key, data, avl_tree);
    return data;
}

void *avl_tree_remove(avl_tree_t *avl_tree, long long int key) {
    void *data;
    tree_remove_node(avl_tree->root, key, &data);
    return data;
}

avl_tree_node_t *avl_tree_get(avl_tree_t *avl_tree, long long int key) {
    return tree_find(avl_tree->root, key);
}

avl_tree_node_t *avl_tree_min(avl_tree_node_t *root) {
    avl_tree_node_t *left = root->left;
    while (left) {
        root = left;
        left = root->left;
    }
    return root;
}

avl_tree_node_t *avl_tree_max(avl_tree_node_t *root) {
    avl_tree_node_t *right = root->right;
    while (right) {
        root = right;
        right = root->right;
    }
    return right;
}

avl_tree_node_t *avl_tree_next(avl_tree_node_t *node) {
    avl_tree_node_t *p;
    if (!node) return NULL;
    if (!node->right) {
        if (tree_is_left(node)) return node->parent;

        p = node->parent;
        while (p && !tree_is_left(p)) p = p->parent;
        if (p && !tree_is_right(p)) p = p->parent;
        return p;
    }

    return avl_tree_min(node->right);
}

avl_tree_node_t *avl_tree_prev(avl_tree_node_t *node) {
    avl_tree_node_t *p;
    if (!node) return NULL;
    if (!node->left) {
        if (tree_is_right(node)) return node->parent;

        p = node->parent;
        while (p && !tree_is_right(p)) p = p->parent;
        if (p && !tree_is_left(p)) p = p->parent;
        return p;
    }

    return avl_tree_max(node->left);
}
