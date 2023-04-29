#include "lib-header/bplustree.h"
#include "lib-header/stdmem.h"

/* B+ Tree */
static struct NodeFileSystem nodes[MAX_NODES]; // Static buffer to hold nodes
static uint32_t node_index = 0; // Current index in the buffer nodes

static struct PCNode pcNodes[MAX_NODES]; // Static buffer to hold PCNodes
static uint32_t pc_node_index = 0; // Current index in the buffer PCNodes

struct NodeFileSystem *make_tree(char *file_name, char* ext, uint32_t parent_cluster_number){
    // Malloc new leaf
    struct NodeFileSystem *newTree = make_leaf();

    // Copy filename to key
    memcpy(newTree->keys[0], file_name, 8);

    // Create new PCNode
    newTree->children[0] = make_pcnode(parent_cluster_number, ext);

    // Set parent to NULL
    newTree->parent = NULL;

    // Increment number of keys
    newTree->number_of_keys++;

    return newTree;
}

struct PCNode *make_pcnode(uint32_t parent_cluster_number, char *ext){
    // Create a new Parent Cluster Node
    struct PCNode *node = &pcNodes[pc_node_index];
    pc_node_index++;

    // Insert parent_cluster_number and extension
    node->parent_cluster_number[0] = parent_cluster_number;
    node->n_of_items = 1;
    memcpy(node->ext[0], ext, 3);

    return node;
}

struct NodeFileSystem *make_node(){
    // Create a new node
    struct NodeFileSystem *node = &nodes[node_index];
    node_index++;

    // Initialize node's attributes
    node->leaf = FALSE;
    node->number_of_keys = 0;
    node->parent = NULL;

    return node;
}

struct NodeFileSystem *make_leaf(){
    // Create a new leaf (leaf == TRUE)
    struct NodeFileSystem *node = make_node();
    node->leaf = TRUE;

    return node;
}

struct PCNode *find_pcn(struct NodeFileSystem *root, char* file_name){
    // Find leaf node that contains key = file_name
    struct NodeFileSystem *leaf = find_leaf(root, file_name);

    // Find key to return PCNode
    uint32_t i = 0;
    for(i = 0; i < leaf->number_of_keys; i++){
        if(memcmp(leaf->keys[i], file_name, 8) == 0){
            break;
        }
    }

    // If key not found (file_name not found)
    if(i == leaf->number_of_keys){
        return NULL;
    } else {
        // Return PCNode
        return (struct PCNode *) leaf->children[i];
    }
}

struct NodeFileSystem *find_leaf(struct NodeFileSystem *root, char *file_name){
    // Find leaf by iterating nodes with key = file_name
    uint32_t i;
    struct NodeFileSystem *temp = root;

    // While node != leaf
    while(!temp->leaf){
        // Iterate all keys
        i = 0;
        while(i < temp->number_of_keys){
            if(memcmp(file_name, temp->keys[i], 8) >= 0){
                i++;
            } else {
                // Key found
                break;
            }
        }
        // Iterate until leaf found
        temp = (struct NodeFileSystem *) temp->children[i];
    }

    return temp;
}

struct NodeFileSystem *insert(struct NodeFileSystem *root, char *file_name, char *ext, uint32_t parent_cluster_number){
    // Make new PCNode
    struct PCNode *newPCN = NULL;
    
    // Search if key already exists
    newPCN = find_pcn(root, file_name);
    if(newPCN != NULL){
        return insert_another_pcn(root, newPCN, parent_cluster_number, ext);
    }

    // Create new PCNode
    newPCN = make_pcnode(parent_cluster_number, ext);

    // Find leaf
    struct NodeFileSystem *leaf = find_leaf(root, file_name);

    // Check if leaf need to be splitted (balancing)
    if(leaf->number_of_keys < MAX_CHILDREN - 1){
        return insert_into_leaf(root, leaf, file_name, newPCN);
    }

    return insert_into_leaf_after_splitting(root, leaf, file_name, newPCN);
}

struct NodeFileSystem *insert_into_leaf(struct NodeFileSystem *root, struct NodeFileSystem *leaf, char *file_name, struct PCNode *newPCN){
    // Find key in leaf
    uint32_t i;
    uint32_t newIdx = 0;

    while(newIdx < leaf->number_of_keys && memcmp(leaf->keys[newIdx], file_name, 8) == -1){
        newIdx++;
    }

    // Shift keys to right to assign the file_name in the correct order (B+ Tree must be sorted!)
    for(i = leaf->number_of_keys; i > newIdx; i--){
        memcpy(leaf->keys[i], leaf->keys[i-1], 8);
        leaf->children[i] = leaf->children[i-1];
    }

    // Assign new key
    memcpy(leaf->keys[newIdx], file_name, 8);
    
    // Assign PCNode to leaf
    leaf->children[newIdx] = newPCN;

    // Increment number of keys
    leaf->number_of_keys++;

    return root;
}

struct NodeFileSystem *insert_another_pcn(struct NodeFileSystem *root, struct PCNode *node, uint32_t parent_cluster_number, char *ext){
    // Insert another target with the same target name
    memcpy(node->ext[node->n_of_items], ext, 3);
    node->parent_cluster_number[node->n_of_items] = parent_cluster_number;
    node->n_of_items++;

    return root;
}

struct NodeFileSystem *insert_into_leaf_after_splitting(struct NodeFileSystem *root, struct NodeFileSystem *leaf, char *file_name, struct PCNode *newPCN){
    // Create new leaf and temporary attributes
    struct NodeFileSystem *new_leaf = make_leaf();
    char keys[MAX_CHILDREN][8];
    void *children[MAX_CHILDREN + 1];
    uint32_t newIdx, split, i , j;
    char new_key[8];

    // Iterate through all keys to find index to split
    newIdx = 0;
    while(newIdx < MAX_CHILDREN - 1 && memcmp(leaf->keys[newIdx], file_name, 8) == -1) {
        newIdx++;
    }

    // Assign leaf keys to temporary 
    for(i = 0, j = 0; i < leaf->number_of_keys; i++, j++){
        if(j == newIdx){
            // Skip index to split
            j++;
        }
        memcpy(keys[j], leaf->keys[i], 8);
        children[j] = leaf->children[i];
    }

    // Assign new key and new PCN to new index
    memcpy(keys[newIdx], file_name, 8);
    children[newIdx] = newPCN;

    // Reset number of keys
    leaf->number_of_keys = 0;

    // Find index to split
    split = ceil(MAX_CHILDREN - 1, 2);

    // Make left leaf
    for(i = 0; i < split; i++){
        leaf->children[i] = children[i];
        memcpy(leaf->keys[i], keys[i], 8);
        leaf->number_of_keys++;
    }

    // Make right leaf
    for(i = split, j = 0; i < MAX_CHILDREN; i++, j++){
        new_leaf->children[j] = children[i];
        memcpy(new_leaf->keys[j], keys[i], 8);
        new_leaf->number_of_keys++;
    }

    // Make pointer from left to right node and right node to itself
    new_leaf->children[MAX_CHILDREN - 1] = leaf->children[MAX_CHILDREN - 1];
    leaf->children[MAX_CHILDREN - 1] = new_leaf;

    // Remove children that have no keys
    for(i = leaf->number_of_keys; i < MAX_CHILDREN - 1; i++){
        leaf->children[i] = NULL;
    }

    for(i = new_leaf->number_of_keys; i < MAX_CHILDREN - 1; i++){
        new_leaf->children[i] = NULL;
    }

    // Assign new leaf parent
    new_leaf->parent = leaf->parent;
    memcpy(new_key, new_leaf->keys[0], 8);

    // Insert into parent after splitting
    return insert_into_parent(root, leaf, new_key, new_leaf);
}

struct NodeFileSystem *insert_into_parent(struct NodeFileSystem *root, struct NodeFileSystem *left, char *file_name, struct NodeFileSystem *right){
    uint32_t left_index;
    struct NodeFileSystem *parent;

    // Get parent from left node
    parent = left->parent;

    // If no parent found
    if (parent == NULL){
        // Create new root
        return insert_into_new_root(left, file_name, right);
    }
    
    // Get left index of left node from parent
    left_index = get_left_index(parent, left);

    // Check if node needs to be splitted (balancing)
    if (parent->number_of_keys < MAX_CHILDREN - 1){
        return insert_into_node(root, parent, left_index, file_name, right);
    }
    
    return insert_into_node_after_splitting(root, parent, left_index, file_name, right);
}

struct NodeFileSystem *insert_into_new_root(struct NodeFileSystem *left, char *file_name, struct NodeFileSystem *right){
    // Create a new root node
    struct NodeFileSystem *newRoot = make_node();

    // Assign values to root's attributes
    memcpy(newRoot->keys[0], file_name, 8);
    newRoot->children[0] = left;
    newRoot->children[1] = right;
    newRoot->number_of_keys++;
    newRoot->parent = NULL;

    // Assign left and right node parent to new root
    left->parent = newRoot;
    right->parent = newRoot;

    return newRoot;
}

struct NodeFileSystem *insert_into_node(struct NodeFileSystem *root, struct NodeFileSystem *n, uint32_t left_index, char *file_name, struct NodeFileSystem *right){
    // Shift right keys and childrens to insert new key
    uint32_t i;
    for(i = n->number_of_keys; i > left_index; i--){
        n->children[i+1] = n->children[i];
        memcpy(n->keys[i], n->keys[i-1], 8);
    }

    // Insert new children to the correct position (must be ordered!)
    n->children[left_index + 1] = right;
    
    // Assign key to the correct position
    memcpy(n->keys[left_index], file_name, 8);

    // Increment number of keys
    n->number_of_keys++;

    return root;
}

struct NodeFileSystem *insert_into_node_after_splitting(struct NodeFileSystem *root, struct NodeFileSystem *old_node, uint32_t left_index, char *file_name, struct NodeFileSystem *right){
    // Create new node and temporary attributes
    uint32_t i, j, split;
    struct NodeFileSystem *new_node, *child;
    char keys[MAX_CHILDREN][8], k_prime[8];
    void *children[MAX_CHILDREN + 1];

    // Assign old children to temporary children
    for (i = 0, j = 0; i < old_node->number_of_keys + 1; i++, j++) {
        if (j == left_index + 1){
            j++;
        }
        children[j] = old_node->children[i];
    }

    // Assign old keys to temporary keys
    for (i = 0, j = 0; i < old_node->number_of_keys; i++, j++) {
        if (j == left_index){
            j++;
        }
        memcpy(keys[j], old_node->keys[i], 8);
    }

    // Assign new key and children
    children[left_index + 1] = right;
    memcpy(keys[left_index], file_name, 8);

    // Find split index
    split = ceil(MAX_CHILDREN, 2);

    // Create new node
    new_node = make_node();

    // Reset old node
    old_node->number_of_keys = 0;

    // Split to left node
    for (i = 0; i < split - 1; i++) {
        old_node->children[i] = children[i];
        memcpy(old_node->keys[i], keys[i], 8);
        old_node->number_of_keys++;
    }

    // Assign children and key prime
    old_node->children[i] = children[i];
    memcpy(k_prime, keys[split - 1], 8);
    i++;

    // Split to right node
    for (j = 0; i < MAX_CHILDREN; i++, j++) {
        new_node->children[j] = children[i];
        memcpy(new_node->keys[j], keys[i], 8);
        new_node->number_of_keys++;
    }

    // Assign children and parent
    new_node->children[j] = children[i];
    new_node->parent = old_node->parent;
    
    // Assign child's parent to new node
    for (i = 0; i <= new_node->number_of_keys; i++) {
        child = new_node->children[i];
        child->parent = new_node;
    }

    // Insert into parent after splitting
    return insert_into_parent(root, old_node, k_prime, new_node);
}

uint32_t get_left_index(struct NodeFileSystem *parent, struct NodeFileSystem *left){
    // Iterate all keys and children of parent to find the left node
    uint32_t left_index = 0;
    while(left_index <= parent->number_of_keys && parent->children[left_index] != left){
        left_index++;
    }

    // Return index of left node
    return left_index;
}

void initialize_b_tree(struct NodeFileSystem *root, char *dir_name, uint32_t parent_cluster_number, uint32_t dir_cluster_number){
    struct FAT32DirectoryTable dir_table[10];
    struct FAT32DriverRequest read_folder_request = {
        .buf = dir_table,
        .ext = "\0\0\0",
        .parent_cluster_number = parent_cluster_number,
        .buffer_size = sizeof(struct FAT32DirectoryTable) * 10,
    };
    memcpy(read_folder_request.name, dir_name, 8);
    int tes = read_directory(read_folder_request);
    if(tes){}
    for (uint32_t i = 0; i < 10; i++)
    {
        uint32_t counter_entry = 0;
        for (uint32_t j = 1; j < CLUSTER_SIZE / sizeof(struct FAT32DirectoryEntry) && counter_entry < dir_table[0].table[0].n_of_entries; j++)
        {
            counter_entry++;
            if (dir_table[i].table[j].attribute == ATTR_SUBDIRECTORY)
            {
                BPlusTree = insert(root, dir_table[i].table[j].name, dir_table[i].table[j].ext, dir_cluster_number);
                root = BPlusTree;
                if(memcmp(dir_name, "root\0\0\0\0", 8) == 0){
                    initialize_b_tree(BPlusTree, dir_table[i].table[j].name, (dir_table[i].table[0].cluster_high << 16) + dir_table[i].table[0].cluster_low, (dir_table[i].table[j].cluster_high << 16) + dir_table[i].table[j].cluster_low);
                } else {
                    initialize_b_tree(BPlusTree, dir_table[i].table[j].name, dir_cluster_number, (dir_table[i].table[j].cluster_high << 16) + dir_table[i].table[j].cluster_low);
                }
            }
            else if (!is_entry_empty(&dir_table[i].table[j]))
            {
                insert(root, dir_table[i].table[j].name, dir_table[i].table[j].ext, dir_cluster_number);
            }
        }
    }
}

uint8_t whereis_main(struct RequestSearch *request){
    // Find PCNode that has target name
    struct PCNode *result = find_pcn(BPlusTree, request->search);

    // If no target found
    if(result == NULL){
        return 0;
    } else {
        // Target found
        request->result = *result;
        return 1;
    }
}

void reset_nodes(){
    for(int i = 0; i < MAX_NODES; i++){
        // Reset Nodes
        for(int j = 0 ; j < MAX_CHILDREN; j++){
            nodes[i].children[i] = NULL;
        }
        nodes[i].number_of_keys = 0;
        nodes[i].leaf = FALSE;
        nodes[i].parent = NULL;

        // Reset PCNodes
        for(int j = 0; j < MAX_SAME_TARGET; j++){
            pcNodes[i].parent_cluster_number[i] = 0;
            memcpy(pcNodes[i].ext, "\0\0\0", 3);
        }
        pcNodes[i].n_of_items = 0;
    }
}

void create_b_tree(){
    reset_nodes();

    // Initialize B+ Tree
    BPlusTree = make_tree("root\0\0\0\0", "\0\0\0", 2);
    initialize_b_tree(BPlusTree, "root\0\0\0\0", 2, 2);
}