#ifndef _BPLUSTREE_H
#define _BPLUSTREE_H

#include "stdtype.h"
#include "fat32.h"

#define MAX_NODES 256
#define MAX_CHILDREN 5
#define NULL ((void *)0)

/* -- B+ Tree -- */
/**
 * @brief Parent Cluster Node, containing parent cluster number of files/folders and the extensions
 * @param parent_cluster_number Parent directory cluster number
 * @param ext Extensions of each file/folder
 * @param n_of_items Number of items (files/folders)
 */
struct PCNode
{
  uint32_t parent_cluster_number[MAX_CHILDREN];
  char ext[MAX_CHILDREN][3];
  uint32_t n_of_items;
};

/**
 * @brief NodeFileSystem, element of B+ Tree
 * @param number_of_keys The amount of all available keys inside a node
 * @param children Children of a node (can be NodeFileSystem or PCNode)
 * @param parent Parent of a node
 * @param leaf Determines if the node is a leaf or not
 */
struct NodeFileSystem
{
  uint32_t number_of_keys;
  char keys[MAX_CHILDREN][8];
  void *children[MAX_CHILDREN + 1];
  struct NodeFileSystem *parent;
  bool leaf;
};

/**
 * @brief RequestSearch, To handle whereis request
 * @param search the target name
 * @param result node containing list of files/folders information to print
 */
struct RequestSearch
{
    char search[8];
    struct PCNode result;
};

// B+ Tree
extern struct NodeFileSystem *BPlusTree;

/**
 * @brief Make a new base tree from root
 * @param file_name the name of the file/directory
 * @param ext the extension of the file/directory
 * @param parent_cluster_number the parent cluster number of the root
 */
struct NodeFileSystem *make_tree(char *file_name, char *ext, uint32_t parent_cluster_number);

/**
 * @brief Make a new Parent Cluster Node
 * @param parent_cluster_number the parent cluster number of the file/directory
 * @param ext the extension of the file/directory
 */
struct PCNode *make_pcnode(uint32_t parent_cluster_number, char *ext);

/**
 * @brief Make a new node
 */
struct NodeFileSystem *make_node();

/**
 * @brief Make a new leaf (leaf = TRUE)
 */
struct NodeFileSystem *make_leaf();

/**
 * @brief Find Parent Cluster Node with target name == file_name
 * @param root the B+ Tree
 * @param file_name the name of the file/directory
 */
struct PCNode *find_pcn(struct NodeFileSystem *root, char* file_name);

/**
 * @brief Find Leaf with target name == file_name
 * @param root the B+ Tree
 * @param file_name the name of the file/directory
 */
struct NodeFileSystem *find_leaf(struct NodeFileSystem *root, char *file_name);

/**
 * @brief Insert a new file/directory to tree
 * @param root the B+ Tree
 * @param file_name the name of the file/directory
 * @param ext the extension of the file/directory
 * @param parent_cluster_number the parent cluster number of the file/directory
 */
struct NodeFileSystem *insert(struct NodeFileSystem *root, char *file_name, char *ext, uint32_t parent_cluster_number);

/**
 * @brief Insert Key and PCNode to Leaf
 * @param root the B+ Tree
 * @param leaf the Leaf Node
 * @param file_name the name of the file/directory
 * @param newPCN New PCNode containing parent cluster number and ext of the file/directory
 */
struct NodeFileSystem *insert_into_leaf(struct NodeFileSystem *root, struct NodeFileSystem *leaf, char *file_name, struct PCNode *newPCN);

/**
 * @brief Insert another file/directory information to PCNode with same target's name
 * @param root the B+ Tree
 * @param node PCNode with the same target's name
 * @param parent_cluster_number the parent cluster number of the file/directory
 * @param ext the extension of the file/directory
 */
struct NodeFileSystem *insert_another_pcn(struct NodeFileSystem *root, struct PCNode *node, uint32_t parent_cluster_number, char *ext);

/**
 * @brief Insert file/directory information into leaf after splliting the leaf
 * @param root the B+ Tree
 * @param leaf the Leaf Node
 * @param file_name the name of the file/directory
 * @param newPCN New PCNode containing parent cluster number and ext of the file/directory
 */
struct NodeFileSystem *insert_into_leaf_after_splitting(struct NodeFileSystem *root, struct NodeFileSystem *leaf, char *file_name, struct PCNode *newPCN);

/**
 * @brief Insert target into parent, if parent == NULL, create a new parent
 * @param root the B+ Tree
 * @param left the left node
 * @param file_name the name of the file/directory
 * @param right the right node
 */
struct NodeFileSystem *insert_into_parent(struct NodeFileSystem *root, struct NodeFileSystem *left, char *file_name, struct NodeFileSystem *right);

/**
 * @brief Create a new root of tree with target
 * @param left the left node
 * @param file_name the name of the file/directory
 * @param right the right node
 */
struct NodeFileSystem *insert_into_new_root(struct NodeFileSystem *left, char *file_name, struct NodeFileSystem *right);

/**
 * @brief Insert target into node
 * @param root the B+ Tree
 * @param n the node that want to be inserted with target
 * @param left_index the index of left node
 * @param file_name the name of the file/directory
 * @param right the right node
 */
struct NodeFileSystem *insert_into_node(struct NodeFileSystem *root, struct NodeFileSystem *n, uint32_t left_index, char *file_name, struct NodeFileSystem *right);

/**
 * @brief Insert target into node after splitting
 * @param root the B+ Tree
 * @param n the node that want to be inserted with target
 * @param left_index the index of left node
 * @param file_name the name of the file/directory
 * @param right the right node
 */
struct NodeFileSystem *insert_into_node_after_splitting(struct NodeFileSystem *root, struct NodeFileSystem *n, uint32_t left_index, char *file_name, struct NodeFileSystem *right);

/**
 * @brief Get the index of left node
 * @param parent the parent node
 * @param left the left node
 */
uint32_t get_left_index(struct NodeFileSystem *parent, struct NodeFileSystem *left);

/**
 * @brief Initialize a new B+ Tree from a directory (root)
 * @param root the B+ Tree
 * @param dir_name the directory name (root)
 * @param parent_cluster_number the parent of the directory
 * @param dir_cluster_number the cluster number of the directory
 */
void initialize_b_tree(struct NodeFileSystem *root, char* dir_name, uint32_t parent_cluster_number, uint32_t dir_cluster_number);

/**
 * @brief Search target with find_pcn
 * @param request RequestSearch containing target's name
 * @return 0 if no target found, 1 if target found
 */
uint8_t whereis_main(struct RequestSearch *request);

#endif
