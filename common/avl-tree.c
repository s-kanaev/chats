#include "avl-tree.h"
#include "memory.h"

#include <stdbool.h>
#include <assert.h>

struct node;

struct avl_tree {
    size_t data_size;
    struct node *root;
};

struct node {
    struct node *left;              ///< less-than value
    struct node *right;             ///< more-than value
    unsigned char height;           ///< balance_factor = height(left) - height(right)
    long value;
};

/******************* node functions ****************/
static
struct node *node_init(long value, avl_tree_t *host) {
    struct node *n = allocate(sizeof(struct node) + host->data_size);
    assert(n);

    n->value = value;
    n->left = n->right = 0;
    n->height = 1;
}

static
void node_deinit(struct node *n) {
    deallocate(n);
}

static
unsigned char height(struct node *n) {
    return n ? n->height : 0;
}

static
int balance_factor(struct node *n) {
    return height(n->right) - height(n->left);
}

static
void fix_node_height(struct node *n) {
    unsigned char hl = height(n->left),
                  hr = height(n->right);
    n->height = (hl > hr ? hl : hr) + 1;
}

/********************* AVL tree functions *******************/
/* rotate around 'p' */
static
struct node *rotate_right(struct node *p) {
    struct node *q = p->left;
    p->left = q->right;
    q->right = p;

    fix_node_height(p);
    fix_node_height(q);

    return q;
}

static
struct node *rotate_left(struct node *q) {
    struct node *p = q->right;
    q->right = p->left;
    p->left = q;

    fix_node_height(q);
    fix_node_height(p);

    return p;
}

static
struct node *balance_tree(struct node *p) {
    fix_node_height(p);

    switch (balance_factor(p)) {
        case +2:
            if (balance_factor(p->right) < 0) p->right = rotate_right(p->right);
            return rotate_left(p);
            break;
        case -2:
            if (balance_factor(p->left) > 0) p->left = rotate_left(p->left);
            return rotate_right(p);
            break;
    }

    return p;
}

static
struct node *insert(struct node *p, long value, avl_tree_t *host) {
    if (!p) return node_init(value, host);

    if (value < p->value) p->left = insert(p->left, value, host);
    else /*if (value > p->value)*/ p->right = insert(p->right, value, host);

    balance_tree(p);
}

static
struct node *find_minimum_node(struct node *p) {
    return p->left ? find_minimum_node(p->left) : p;
}

static
struct node *remove_minimum_node(struct node *p) {
    if (!p->left) return p->right;
    p->left = remove_minimum_node(p->left);

    return balance_tree(p);
}

static
struct node *remove_node(struct node *p, long value) {
    if (!p) return NULL;
    if (value < p->value) p->left = remove_node(p->left, value);
    else if (value > p->value) p->right = remove_node(p->right, value);
    else {
        struct node *q = p->left,
                    *r = p->right,
                    *min;
        node_deinit(p);

        if (!r) return q;
        min = find_minimum_node(r);
        min->right = find_minimum_node(r);
        min->left = q;

        return balance_tree(min);
    }

    return balance_tree(p);
}

static
void purge_node_recursive(struct node *te) {
    if (te->left) purge_node_recursive(te->left);
    if (te->right) purge_node_recursive(te->right);
    deallocate(te);
}

static
struct node *find_node(struct node *te, long value) {
    if (te->value < value) {
        if (!te->left) return NULL;
        return find_node(te->left, value);
    }
    else if (te->value > value) {
        if (!te->right) return NULL;
        return find_node(te->right, value);
    }
    return te;
}

/***************** API *************************/
avl_tree_t *avl_tree_init(size_t data_size) {
    avl_tree_t *avl_tree = allocate(sizeof(avl_tree_t));

    if (!avl_tree) return NULL;

    avl_tree->data_size = data_size;
    avl_tree->root = NULL;

    return avl_tree;
}

void avl_tree_deinit(avl_tree_t *avl_tree) {
    if (!avl_tree) return;
    if (avl_tree->root) purge_node_recursive(avl_tree->root);
    deallocate(avl_tree);
}

void *avl_tree_get(avl_tree_t *avl_tree, long int value) {
    if (!avl_tree) return;
    if (!avl_tree->root) return NULL;

    return (find_node(avl_tree->root, value) + 1);
}

void *avl_tree_add(avl_tree_t *avl_tree, long int value) {
    if (!avl_tree) return;
    avl_tree->root = insert(avl_tree->root, value, avl_tree);
    return (find_node(avl_tree->root, value) + 1);
}

bool avl_tree_remove_by_value(avl_tree_t *avl_tree, long int value) {
    if (!avl_tree) return false;
    remove_node(avl_tree->root, value);
    return true;
}

bool avl_tree_remove_by_data(avl_tree_t *avl_tree, void *data) {
    struct node *n;
    if (!avl_tree || !data) return false;
    n = data - sizeof(struct node);
    return avl_tree_remove_by_value(avl_tree->root, n->value);
}
