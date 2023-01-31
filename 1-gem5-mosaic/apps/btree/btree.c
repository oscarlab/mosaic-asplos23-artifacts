/*
 *  bpt.c
 */
#define Version "1.16.1"
/*
 *
 *  bpt:  B+ Tree Implementation
 *
 *  Copyright (c) 2018  Amittai Aviram  http://www.amittai.com
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice,
 *  this list of conditions and the following disclaimer.
 *
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *  this list of conditions and the following disclaimer in the documentation
 *  and/or other materials provided with the distribution.

 *  3. The name of the copyright holder may not be used to endorse
 *  or promote products derived from this software without specific
 *  prior written permission.

 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.

 *  Author:  Amittai Aviram
 *    http://www.amittai.com
 *    amittai.aviram@gmail.com or afa13@columbia.edu
 *  Original Date:  26 June 2010
 *  Last modified: 02 September 2018
 *
 *  This implementation demonstrates the B+ tree data structure
 *  for educational purposes, includin insertion, deletion, search, and display
 *  of the search path, the leaves, or the whole tree.
 *
 *  Must be compiled with a C99-compliant C compiler such as the latest GCC.
 *
 *  Usage:  bpt [order]
 *  where order is an optional argument
 *  (integer MIN_ORDER <= order <= MAX_ORDER)
 *  defined as the maximal number of pointers in any node.
 *
 */

///< the name of the shared memory file created
#define CONFIG_SHM_FILE_NAME "/tmp/alloctest-bench"


#ifdef _OPENMP
///< the number of elements in the tree
//#define NELEMENTS (8UL << 30)
#define NELEMENTS   (1400UL << 20)
///< the number of lookups
#define NLOOKUP 2000000000UL
#else
///< the number of elements in the tree
//#define NELEMENTS (3400UL << 20)

#define NELEMENTS (100UL << 20)
#define NLOOKUP 10000000
///< the number of lookups
//#define NLOOKUP 50000000
#endif



#include <stdbool.h>
#ifdef _WIN32
#define bool char
#define false 0
#define true 1
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/time.h>
#include <string.h>

#ifdef _OPENMP
#include <omp.h>
#endif

// Default order is 4.
#define DEFAULT_ORDER 4

// Minimum order is necessarily 3.  We set the maximum
// order arbitrarily.  You may change the maximum order.
#define MIN_ORDER 3
#define MAX_ORDER 20

// Constant for optional command-line input with "i" command.
#define BUFFER_SIZE 256


#define ALIGNMET (1UL << 21)

size_t allocator_stat = 0;

int cnt = 0;

///> this allocates memory aligned to a large page size
static inline void *allocate(size_t size)
{
	void *memptr;
	if (posix_memalign(&memptr, ALIGNMET, size)) {
		printf("ENOMEM\n");
		exit(1);
	}

	allocator_stat += size;

	memset(memptr, 0, size);
	return memptr;
}

///> this allocates cache-line sized memory
static inline void *allocate_align64(size_t size)
{
	void *memptr;
	//printf("allocating %zu kB memory\n", size >> 10);
	if (posix_memalign(&memptr, 64, size)) {
		printf("ENOMEM\n");
		exit(1);
	}

	allocator_stat += size;

	memset(memptr, 0, size);
	return memptr;
}

// TYPES.

/* Type representing the record
 * to which a given key refers.
 * In a real B+ tree system, the
 * record would hold data (in a database)
 * or a file (in an operating system)
 * or some other information.
 * Users can rewrite this part of the code
 * to change the type and content
 * of the value field.
 */
typedef struct record {
	union {
		uint64_t value;
		struct record *next;
	};
	uint64_t flags;
} record;

/* Type representing a node in the B+ tree.
 * This type is general enough to serve for both
 * the leaf and the internal node.
 * The heart of the node is the array
 * of keys and the array of corresponding
 * pointers.  The relation between keys
 * and pointers differs between leaves and
 * internal nodes.  In a leaf, the index
 * of each key equals the index of its corresponding
 * pointer, with a maximum of order - 1 key-pointer
 * pairs.  The last pointer points to the
 * leaf to the right (or NULL in the case
 * of the rightmost leaf).
 * In an internal node, the first pointer
 * refers to lower nodes with keys less than
 * the smallest key in the keys array.  Then,
 * with indices i starting at 0, the pointer
 * at i + 1 points to the subtree with keys
 * greater than or equal to the key in this
 * node at index i.
 * The num_keys field is used to keep
 * track of the number of valid keys.
 * In an internal node, the number of valid
 * pointers is always num_keys + 1.
 * In a leaf, the number of valid pointers
 * to data is always num_keys.  The
 * last leaf pointer points to the next leaf.
 */
typedef struct node {
	void ** pointers;
	uint64_t * keys;
	struct node * parent;
	bool is_leaf;
	uint64_t num_keys;
	struct node * next; // Used for queue.
} node;


// GLOBALS.

/* The order determines the maximum and minimum
 * number of entries (keys and pointers) in any
 * node.  Every node has at most order - 1 keys and
 * at least (roughly speaking) half that number.
 * Every leaf has as many pointers to data as keys,
 * and every internal node has one more pointer
 * to a subtree than the number of keys.
 * This global variable is initialized to the
 * default value.
 */
uint64_t order = DEFAULT_ORDER;

/* The queue is used to pruint64_t the tree in
 * level order, starting from the root
 * printing each entire rank on a separate
 * line, finishing with the leaves.
 */
node * queue = NULL;

/* The user can toggle on and off the "verbose"
 * property, which causes the pointer addresses
 * to be printed out in hexadecimal notation
 * next to their corresponding keys.
 */
bool verbose_output = false;


// FUNCTION PROTOTYPES.

// Output and utility.

void license_notice(void);
void usage_1(void);
void usage_2(void);
void usage_3(void);
void enqueue(node * new_node);
node * dequeue(void);
uint64_t height(node * const root);
uint64_t path_to_root(node * const root, node * child);
void print_leaves(node * const root);
void print_tree(node * const root);
void find_and_print(node * const root, uint64_t key, bool verbose);
void find_and_print_range(node * const root, uint64_t range1, uint64_t range2, bool verbose);
uint64_t find_range(node * const root, uint64_t key_start, uint64_t key_end, bool verbose,
		uint64_t returned_keys[], void * returned_pointers[]);
node * find_leaf(node * const root, uint64_t key, bool verbose);
record * find(node * root, uint64_t key, bool verbose, node ** leaf_out);
uint64_t cut(uint64_t length);

// Insertion.

record * make_record(uint64_t value);
node * make_node(void);
node * make_leaf(void);
uint64_t get_left_index(node * parent, node * left);
node * insert_into_leaf(node * leaf, uint64_t key, record * pointer);
node * insert_into_leaf_after_splitting(node * root, node * leaf, uint64_t key,
		record * pointer);
node * insert_into_node(node * root, node * parent,
		uint64_t left_index, uint64_t key, node * right);
node * insert_into_node_after_splitting(node * root, node * parent,
		uint64_t left_index,
		uint64_t key, node * right);
node * insert_into_parent(node * root, node * left, uint64_t key, node * right);
node * insert_into_new_root(node * left, uint64_t key, node * right);
node * start_new_tree(uint64_t key, record * pointer);
node * insert(node * root, uint64_t key, uint64_t value);

// Deletion.

uint64_t get_neighbor_index(node * n);
node * adjust_root(node * root);
node * coalesce_nodes(node * root, node * n, node * neighbor,
		uint64_t neighbor_index, uint64_t k_prime);
node * redistribute_nodes(node * root, node * n, node * neighbor,
		uint64_t neighbor_index,
		uint64_t k_prime_index, uint64_t k_prime);
node * delete_entry(node * root, node * n, uint64_t key, void * pointer);
node * delete(node * root, uint64_t key);




// FUNCTION DEFINITIONS.

// OUTPUT AND UTILITIES

/* Copyright and license notice to user at startup.
*/
void license_notice(void) {
	printf("bpt version %s -- Copyright (c) 2018  Amittai Aviram "
			"http://www.amittai.com\n", Version);
	printf("This program comes with ABSOLUTELY NO WARRANTY.\n"
			"This is free software, and you are welcome to redistribute it\n"
			"under certain conditions.\n"
			"Please see the headnote in the source code for details.\n");
}


/* First message to the user.
*/
void usage_1(void) {
	printf("B+ Tree of Order %ld.\n", order);
	printf("Following Silberschatz, Korth, Sidarshan, Database Concepts, "
			"5th ed.\n\n"
			"To build a B+ tree of a different order, start again and enter "
			"the order\n"
			"as an integer argument:  bpt <order>  ");
	printf("(%d <= order <= %d).\n", MIN_ORDER, MAX_ORDER);
	printf("To start with input from a file of newline-delimited integers, \n"
			"start again and enter the order followed by the filename:\n"
			"bpt <order> <inputfile> .\n");
}


/* Second message to the user.
*/
void usage_2(void) {
	printf("Enter any of the following commands after the prompt > :\n"
			"\ti <k>  -- Insert <k> (an integer) as both key and value).\n"
			"\ti <k> <v> -- Insert the value <v> (an integer) as the value of key <k> (an integer).\n"
			"\tf <k>  -- Find the value under key <k>.\n"
			"\tp <k> -- Pruint64_t the path from the root to key k and its associated "
			"value.\n"
			"\tr <k1> <k2> -- Pruint64_t the keys and values found in the range "
			"[<k1>, <k2>\n"
			"\td <k>  -- Delete key <k> and its associated value.\n"
			"\tx -- Destroy the whole tree.  Start again with an empty tree of the "
			"same order.\n"
			"\tt -- Pruint64_t the B+ tree.\n"
			"\tl -- Pruint64_t the keys of the leaves (bottom row of the tree).\n"
			"\tv -- Toggle output of pointer addresses (\"verbose\") in tree and "
			"leaves.\n"
			"\tq -- Quit. (Or use Ctl-D or Ctl-C.)\n"
			"\t? -- Pruint64_t this help message.\n");
}


/* Brief usage note.
*/
void usage_3(void) {
	printf("Usage: ./bpt [<order>]\n");
	printf("\twhere %d <= order <= %d .\n", MIN_ORDER, MAX_ORDER);
}


/* Helper function for printing the
 * tree out.  See print_tree.
 */
void enqueue(node * new_node) {
	node * c;
	if (queue == NULL) {
		queue = new_node;
		queue->next = NULL;
	}
	else {
		c = queue;
		while(c->next != NULL) {
			c = c->next;
		}
		c->next = new_node;
		new_node->next = NULL;
	}
}


/* Helper function for printing the
 * tree out.  See print_tree.
 */
node * dequeue(void) {
	node * n = queue;
	queue = queue->next;
	n->next = NULL;
	return n;
}


/* Prints the bottom row of keys
 * of the tree (with their respective
 * pointers, if the verbose_output flag is set.
 */
void print_leaves(node * const root) {
	if (root == NULL) {
		printf("Empty tree.\n");
		return;
	}
	uint64_t i;
	node * c = root;
	while (!c->is_leaf)
		c = c->pointers[0];
	while (true) {
		for (i = 0; i < c->num_keys; i++) {
			if (verbose_output)
				printf("%p ", c->pointers[i]);
			printf("%ld ", c->keys[i]);
		}
		if (verbose_output)
			printf("%p ", c->pointers[order - 1]);
		if (c->pointers[order - 1] != NULL) {
			printf(" | ");
			c = c->pointers[order - 1];
		}
		else
			break;
	}
	printf("\n");
}


/* Utility function to give the height
 * of the tree, which length in number of edges
 * of the path from the root to any leaf.
 */
uint64_t height(node * const root) {
	uint64_t h = 0;
	node * c = root;
	while (!c->is_leaf) {
		c = c->pointers[0];
		h++;
	}
	return h;
}


/* Utility function to give the length in edges
 * of the path from any node to the root.
 */
uint64_t path_to_root(node * const root, node * child) {
	uint64_t length = 0;
	node * c = child;
	while (c != root) {
		c = c->parent;
		length++;
	}
	return length;
}


/* Prints the B+ tree in the command
 * line in level (rank) order, with the
 * keys in each node and the '|' symbol
 * to separate nodes.
 * With the verbose_output flag set.
 * the values of the pointers corresponding
 * to the keys also appear next to their respective
 * keys, in hexadecimal notation.
 */
void print_tree(node * const root) {

	node * n = NULL;
	uint64_t i = 0;
	uint64_t rank = 0;
	uint64_t new_rank = 0;

	if (root == NULL) {
		printf("Empty tree.\n");
		return;
	}
	queue = NULL;
	enqueue(root);
	while(queue != NULL) {
		n = dequeue();
		if (n->parent != NULL && n == n->parent->pointers[0]) {
			new_rank = path_to_root(root, n);
			if (new_rank != rank) {
				rank = new_rank;
				printf("\n");
			}
		}
		if (verbose_output)
			printf("(%p)", n);
		for (i = 0; i < n->num_keys; i++) {
			if (verbose_output)
				printf("%p ", n->pointers[i]);
			printf("%ld ", n->keys[i]);
		}
		if (!n->is_leaf)
			for (i = 0; i <= n->num_keys; i++)
				enqueue(n->pointers[i]);
		if (verbose_output) {
			if (n->is_leaf)
				printf("%p ", n->pointers[order - 1]);
			else
				printf("%p ", n->pointers[n->num_keys]);
		}
		printf("| ");
	}
	printf("\n");
}


/* Finds the record under a given key and prints an
 * appropriate message to stdout.
 */
void find_and_print(node * const root, uint64_t key, bool verbose) {
	record * r = find(root, key, verbose, NULL);
	if (r == NULL)
		printf("Record not found under key %ld.\n", key);
	else
		printf("Record at %p -- key %ld, value %ld.\n",
				r, key, r->value);
}


/* Finds and prints the keys, pointers, and values within a range
 * of keys between key_start and key_end, including both bounds.
 */
void find_and_print_range(node * const root, uint64_t key_start, uint64_t key_end,
		bool verbose) {
	uint64_t i;
	uint64_t array_size = key_end - key_start + 1;
	uint64_t returned_keys[array_size];
	void * returned_pointers[array_size];
	uint64_t num_found = find_range(root, key_start, key_end, verbose,
			returned_keys, returned_pointers);
	if (!num_found)
		printf("None found.\n");
	else {
		for (i = 0; i < num_found; i++)
			printf("Key: %ld   Location: %p  Value: %ld\n",
					returned_keys[i],
					returned_pointers[i],
					((record *)
					 returned_pointers[i])->value);
	}
}


/* Finds keys and their pointers, if present, in the range specified
 * by key_start and key_end, inclusive.  Places these in the arrays
 * returned_keys and returned_pointers, and returns the number of
 * entries found.
 */
uint64_t find_range(node * const root, uint64_t key_start, uint64_t key_end, bool verbose,
		uint64_t returned_keys[], void * returned_pointers[]) {
	uint64_t i, num_found;
	num_found = 0;
	node * n = find_leaf(root, key_start, verbose);
	if (n == NULL) return 0;
	for (i = 0; i < n->num_keys && n->keys[i] < key_start; i++) ;
	if (i == n->num_keys) return 0;
	while (n != NULL) {
		for (; i < n->num_keys && n->keys[i] <= key_end; i++) {
			returned_keys[num_found] = n->keys[i];
			returned_pointers[num_found] = n->pointers[i];
			num_found++;
		}
		n = n->pointers[order - 1];
		i = 0;
	}
	return num_found;
}


/* Traces the path from the root to a leaf, searching
 * by key.  Displays information about the path
 * if the verbose flag is set.
 * Returns the leaf containing the given key.
 */
node * find_leaf(node * const root, uint64_t key, bool verbose) {
	if (root == NULL) {
		if (verbose)
			printf("Empty tree.\n");
		return root;
	}
	uint64_t i = 0;
	node * c = root;
	while (!c->is_leaf) {
		if (verbose) {
			printf("[");
			for (i = 0; i < c->num_keys - 1; i++)
				printf("%ld ", c->keys[i]);
			printf("%ld] ", c->keys[i]);
		}
		i = 0;
		while (i < c->num_keys) {
			if (key >= c->keys[i]) i++;
			else break;
		}
		if (verbose)
			printf("%ld ->\n", i);
		c = (node *)c->pointers[i];
	}
	if (verbose) {
		printf("Leaf [");
		for (i = 0; i < c->num_keys - 1; i++)
			printf("%ld ", c->keys[i]);
		printf("%ld] ->\n", c->keys[i]);
	}
	return c;
}


/* Finds and returns the record to which
 * a key refers.
 */
record * find(node * root, uint64_t key, bool verbose, node ** leaf_out) {
	if (root == NULL) {
		if (leaf_out != NULL) {
			*leaf_out = NULL;
		}
		return NULL;
	}

	uint64_t i = 0;
	node * leaf = NULL;

	leaf = find_leaf(root, key, verbose);

	/* If root != NULL, leaf must have a value, even
	 * if it does not contain the desired key.
	 * (The leaf holds the range of keys that would
	 * include the desired key.)
	 */

	for (i = 0; i < leaf->num_keys; i++)
		if (leaf->keys[i] == key) break;
	if (leaf_out != NULL) {
		*leaf_out = leaf;
	}
	if (i == leaf->num_keys)
		return NULL;
	else
		return (record *)leaf->pointers[i];
}

/* Finds the appropriate place to
 * split a node that is too big into two.
 */
uint64_t cut(uint64_t length) {
	if (length % 2 == 0)
		return length/2;
	else
		return length/2 + 1;
}


// INSERTION


#define NODE_SLAB_GROW (1<<18)

struct node *free_nodes = NULL;

node *alloc_node()
{
	if (!free_nodes) {
		node *n = allocate(NODE_SLAB_GROW);
		for (size_t i = 0; i <  NODE_SLAB_GROW / sizeof(struct node); i ++) {
			n[i].next = free_nodes;
			free_nodes = &n[i];
		}
	}
	node *nd = free_nodes;
	free_nodes = nd->next;

	return nd;
}

void free_node(node *n)
{
	n->next = free_nodes;
	free_nodes = n;
}


#define RECORD_SLAB_GROW (1<<18)

struct record *free_recs = NULL;

record *alloc_record()
{
	if (!free_recs) {
		record *r = allocate(RECORD_SLAB_GROW);
		for (size_t i = 0; i <  RECORD_SLAB_GROW / sizeof(struct record); i ++) {
			r[i].next = free_recs;
			free_recs = &r[i];
		}
	}

	record *rec = free_recs;
	free_recs = rec->next;

	return rec;
}

void free_record(record *r)
{
	r->next = free_recs;
	free_recs = r;
}


/* Creates a new record to hold the value
 * to which a key refers.
 */
record * make_record(uint64_t value) {
	record * new_record = (record *)alloc_record(sizeof(record));
	if (new_record == NULL) {
		perror("Record creation.");
		exit(EXIT_FAILURE);
	}
	else {
		new_record->value = value;
	}
	return new_record;
}



/* Creates a new general node, which can be adapted
 * to serve as either a leaf or an internal node.
 */
node * make_node(void) {
	node * new_node;
	new_node = alloc_node(sizeof(node));
	if (new_node == NULL) {
		perror("Node creation.");
		exit(EXIT_FAILURE);
	}
	new_node->keys = allocate_align64((order - 1) * sizeof(uint64_t));
	if (new_node->keys == NULL) {
		perror("New node keys array.");
		exit(EXIT_FAILURE);
	}
	new_node->pointers = allocate_align64(order * sizeof(void *));
	if (new_node->pointers == NULL) {
		perror("New node pointers array.");
		exit(EXIT_FAILURE);
	}
	new_node->is_leaf = false;
	new_node->num_keys = 0;
	new_node->parent = NULL;
	new_node->next = NULL;
	return new_node;
}

/* Creates a new leaf by creating a node
 * and then adapting it appropriately.
 */
node * make_leaf(void) {
	node * leaf = make_node();
	leaf->is_leaf = true;
	return leaf;
}


/* Helper function used in insert_into_parent
 * to find the index of the parent's pointer to
 * the node to the left of the key to be inserted.
 */
uint64_t get_left_index(node * parent, node * left) {

	uint64_t left_index = 0;
	while (left_index <= parent->num_keys &&
			parent->pointers[left_index] != left)
		left_index++;
	return left_index;
}

/* Inserts a new pointer to a record and its corresponding
 * key into a leaf.
 * Returns the altered leaf.
 */
node * insert_into_leaf(node * leaf, uint64_t key, record * pointer) {

	uint64_t i, insertion_point;

	insertion_point = 0;
	while (insertion_point < leaf->num_keys && leaf->keys[insertion_point] < key)
		insertion_point++;

	for (i = leaf->num_keys; i > insertion_point; i--) {
		leaf->keys[i] = leaf->keys[i - 1];
		leaf->pointers[i] = leaf->pointers[i - 1];
	}
	leaf->keys[insertion_point] = key;
	leaf->pointers[insertion_point] = pointer;
	leaf->num_keys++;
	return leaf;
}


/* Inserts a new key and pointer
 * to a new record into a leaf so as to exceed
 * the tree's order, causing the leaf to be split
 * in half.
 */
node * insert_into_leaf_after_splitting(node * root, node * leaf, uint64_t key, record * pointer) {

	node * new_leaf;
	uint64_t * temp_keys;
	void ** temp_pointers;
	uint64_t insertion_index, split, new_key, i, j;

	new_leaf = make_leaf();

	temp_keys = allocate_align64((order+1) * sizeof(uint64_t));
	if (temp_keys == NULL) {
		perror("Temporary keys array.");
		exit(EXIT_FAILURE);
	}

	temp_pointers = allocate_align64((order+1) * sizeof(void *));
	if (temp_pointers == NULL) {
		perror("Temporary pointers array.");
		exit(EXIT_FAILURE);
	}

	insertion_index = 0;
	while (insertion_index < order - 1 && leaf->keys[insertion_index] < key)
		insertion_index++;

	for (i = 0, j = 0; i < leaf->num_keys; i++, j++) {
		if (j == insertion_index) j++;
		temp_keys[j] = leaf->keys[i];
		temp_pointers[j] = leaf->pointers[i];
	}

	temp_keys[insertion_index] = key;
	temp_pointers[insertion_index] = pointer;

	leaf->num_keys = 0;

	split = cut(order - 1);
	for (i = 0; i < split; i++) {
		leaf->pointers[i] = temp_pointers[i];
		leaf->keys[i] = temp_keys[i];
		leaf->num_keys++;
	}

	for (i = split, j = 0; i < order; i++, j++) {
		new_leaf->pointers[j] = temp_pointers[i];
		new_leaf->keys[j] = temp_keys[i];
		new_leaf->num_keys++;
	}

	free(temp_pointers);
	free(temp_keys);

	new_leaf->pointers[order - 1] = leaf->pointers[order - 1];
	leaf->pointers[order - 1] = new_leaf;

	for (i = leaf->num_keys; i < order - 1; i++)
		leaf->pointers[i] = NULL;
	for (i = new_leaf->num_keys; i < order - 1; i++)
		new_leaf->pointers[i] = NULL;

	new_leaf->parent = leaf->parent;
	new_key = new_leaf->keys[0];

	return insert_into_parent(root, leaf, new_key, new_leaf);
}


/* Inserts a new key and pointer to a node
 * into a node into which these can fit
 * without violating the B+ tree properties.
 */
node * insert_into_node(node * root, node * n,
		uint64_t left_index, uint64_t key, node * right) {
	uint64_t i;

	for (i = n->num_keys; i > left_index; i--) {
		n->pointers[i + 1] = n->pointers[i];
		n->keys[i] = n->keys[i - 1];
	}
	n->pointers[left_index + 1] = right;
	n->keys[left_index] = key;
	n->num_keys++;
	return root;
}


/* Inserts a new key and pointer to a node
 * into a node, causing the node's size to exceed
 * the order, and causing the node to split into two.
 */
node * insert_into_node_after_splitting(node * root, node * old_node, uint64_t left_index,
		uint64_t key, node * right) {

	uint64_t i, j, split, k_prime;
	node * new_node, * child;
	uint64_t * temp_keys;
	node ** temp_pointers;

	/* First create a temporary set of keys and pointers
	 * to hold everything in order, including
	 * the new key and pointer, inserted in their
	 * correct places.
	 * Then create a new node and copy half of the
	 * keys and pointers to the old node and
	 * the other half to the new.
	 */

	temp_pointers = allocate_align64((order + 1) * sizeof(node *));
	if (temp_pointers == NULL) {
		perror("Temporary pointers array for splitting nodes.");
		exit(EXIT_FAILURE);
	}
	temp_keys = allocate_align64(order * sizeof(uint64_t));
	if (temp_keys == NULL) {
		perror("Temporary keys array for splitting nodes.");
		exit(EXIT_FAILURE);
	}

	for (i = 0, j = 0; i < old_node->num_keys + 1; i++, j++) {
		if (j == left_index + 1) j++;
		temp_pointers[j] = old_node->pointers[i];
	}

	for (i = 0, j = 0; i < old_node->num_keys; i++, j++) {
		if (j == left_index) j++;
		temp_keys[j] = old_node->keys[i];
	}

	temp_pointers[left_index + 1] = right;
	temp_keys[left_index] = key;

	/* Create the new node and copy
	 * half the keys and pointers to the
	 * old and half to the new.
	 */
	split = cut(order);
	new_node = make_node();
	old_node->num_keys = 0;
	for (i = 0; i < split - 1; i++) {
		old_node->pointers[i] = temp_pointers[i];
		old_node->keys[i] = temp_keys[i];
		old_node->num_keys++;
	}
	old_node->pointers[i] = temp_pointers[i];
	k_prime = temp_keys[split - 1];
	for (++i, j = 0; i < order; i++, j++) {
		new_node->pointers[j] = temp_pointers[i];
		new_node->keys[j] = temp_keys[i];
		new_node->num_keys++;
	}
	new_node->pointers[j] = temp_pointers[i];
	free(temp_pointers);
	free(temp_keys);
	new_node->parent = old_node->parent;
	for (i = 0; i <= new_node->num_keys; i++) {
		child = new_node->pointers[i];
		child->parent = new_node;
	}

	/* Insert a new key into the parent of the two
	 * nodes resulting from the split, with
	 * the old node to the left and the new to the right.
	 */

	return insert_into_parent(root, old_node, k_prime, new_node);
}



/* Inserts a new node (leaf or internal node) into the B+ tree.
 * Returns the root of the tree after insertion.
 */
node * insert_into_parent(node * root, node * left, uint64_t key, node * right) {

	uint64_t left_index;
	node * parent;

	parent = left->parent;

	/* Case: new root. */

	if (parent == NULL)
		return insert_into_new_root(left, key, right);

	/* Case: leaf or node. (Remainder of
	 * function body.)
	 */

	/* Find the parent's pointer to the left
	 * node.
	 */

	left_index = get_left_index(parent, left);


	/* Simple case: the new key fits into the node.
	*/

	if (parent->num_keys < order - 1)
		return insert_into_node(root, parent, left_index, key, right);

	/* Harder case:  split a node in order
	 * to preserve the B+ tree properties.
	 */

	return insert_into_node_after_splitting(root, parent, left_index, key, right);
}


/* Creates a new root for two subtrees
 * and inserts the appropriate key into
 * the new root.
 */
node * insert_into_new_root(node * left, uint64_t key, node * right) {

	node * root = make_node();
	root->keys[0] = key;
	root->pointers[0] = left;
	root->pointers[1] = right;
	root->num_keys++;
	root->parent = NULL;
	left->parent = root;
	right->parent = root;
	return root;
}



/* First insertion:
 * start a new tree.
 */
node * start_new_tree(uint64_t key, record * pointer) {

	node * root = make_leaf();
	root->keys[0] = key;
	root->pointers[0] = pointer;
	root->pointers[order - 1] = NULL;
	root->parent = NULL;
	root->num_keys++;
	return root;
}



/* Master insertion function.
 * Inserts a key and an associated value into
 * the B+ tree, causing the tree to be adjusted
 * however necessary to maintain the B+ tree
 * properties.
 */
node * insert(node * root, uint64_t key, uint64_t value) {

	record * record_pointer = NULL;
	node * leaf = NULL;

	/* The current implementation ignores
	 * duplicates.
	 */

	record_pointer = find(root, key, false, NULL);
	if (record_pointer != NULL) {

		/* If the key already exists in this tree, update
		 * the value and return the tree.
		 */

		record_pointer->value = value;
		return root;
	}

	/* Create a new record for the
	 * value.
	 */
	record_pointer = make_record(value);


	/* Case: the tree does not exist yet.
	 * Start a new tree.
	 */

	if (root == NULL)
		return start_new_tree(key, record_pointer);


	/* Case: the tree already exists.
	 * (Rest of function body.)
	 */

	leaf = find_leaf(root, key, false);

	/* Case: leaf has room for key and record_pointer.
	*/

	if (leaf->num_keys < order - 1) {
		leaf = insert_into_leaf(leaf, key, record_pointer);
		return root;
	}


	/* Case:  leaf must be split.
	*/

	return insert_into_leaf_after_splitting(root, leaf, key, record_pointer);
}




// DELETION.

/* Utility function for deletion.  Retrieves
 * the index of a node's nearest neighbor (sibling)
 * to the left if one exists.  If not (the node
 * is the leftmost child), returns -1 to signify
 * this special case.
 */
uint64_t get_neighbor_index(node * n) {

	uint64_t i;

	/* Return the index of the key to the left
	 * of the pointer in the parent pointing
	 * to n.
	 * If n is the leftmost child, this means
	 * return -1.
	 */
	for (i = 0; i <= n->parent->num_keys; i++)
		if (n->parent->pointers[i] == n)
			return i - 1;

	// Error state.
	printf("Search for nonexistent pointer to node in parent.\n");
	printf("Node:  %#lx\n", (unsigned long)n);
	exit(EXIT_FAILURE);
}


node * remove_entry_from_node(node * n, uint64_t key, node * pointer) {

	uint64_t i, num_pointers;

	// Remove the key and shift other keys accordingly.
	i = 0;
	while (n->keys[i] != key)
		i++;
	for (++i; i < n->num_keys; i++)
		n->keys[i - 1] = n->keys[i];

	// Remove the pointer and shift other pointers accordingly.
	// First determine number of pointers.
	num_pointers = n->is_leaf ? n->num_keys : n->num_keys + 1;
	i = 0;
	while (n->pointers[i] != pointer)
		i++;
	for (++i; i < num_pointers; i++)
		n->pointers[i - 1] = n->pointers[i];


	// One key fewer.
	n->num_keys--;

	// Set the other pointers to NULL for tidiness.
	// A leaf uses the last pointer to pouint64_t to the next leaf.
	if (n->is_leaf)
		for (i = n->num_keys; i < order - 1; i++)
			n->pointers[i] = NULL;
	else
		for (i = n->num_keys + 1; i < order; i++)
			n->pointers[i] = NULL;

	return n;
}


node * adjust_root(node * root) {

	node * new_root;

	/* Case: nonempty root.
	 * Key and pointer have already been deleted,
	 * so nothing to be done.
	 */

	if (root->num_keys > 0)
		return root;

	/* Case: empty root.
	*/

	// If it has a child, promote
	// the first (only) child
	// as the new root.

	if (!root->is_leaf) {
		new_root = root->pointers[0];
		new_root->parent = NULL;
	}

	// If it is a leaf (has no children),
	// then the whole tree is empty.

	else
		new_root = NULL;

	free(root->keys);
	free(root->pointers);
	free_node(root);

	return new_root;
}


/* Coalesces a node that has become
 * too small after deletion
 * with a neighboring node that
 * can accept the additional entries
 * without exceeding the maximum.
 */
node * coalesce_nodes(node * root, node * n, node * neighbor, uint64_t neighbor_index, uint64_t k_prime) {

	uint64_t i, j, neighbor_insertion_index, n_end;
	node * tmp;

	/* Swap neighbor with node if node is on the
	 * extreme left and neighbor is to its right.
	 */

	if (neighbor_index == -1) {
		tmp = n;
		n = neighbor;
		neighbor = tmp;
	}

	/* Starting pouint64_t in the neighbor for copying
	 * keys and pointers from n.
	 * Recall that n and neighbor have swapped places
	 * in the special case of n being a leftmost child.
	 */

	neighbor_insertion_index = neighbor->num_keys;

	/* Case:  nonleaf node.
	 * Append k_prime and the following pointer.
	 * Append all pointers and keys from the neighbor.
	 */

	if (!n->is_leaf) {

		/* Append k_prime.
		*/

		neighbor->keys[neighbor_insertion_index] = k_prime;
		neighbor->num_keys++;


		n_end = n->num_keys;

		for (i = neighbor_insertion_index + 1, j = 0; j < n_end; i++, j++) {
			neighbor->keys[i] = n->keys[j];
			neighbor->pointers[i] = n->pointers[j];
			neighbor->num_keys++;
			n->num_keys--;
		}

		/* The number of pointers is always
		 * one more than the number of keys.
		 */

		neighbor->pointers[i] = n->pointers[j];

		/* All children must now pouint64_t up to the same parent.
		*/

		for (i = 0; i < neighbor->num_keys + 1; i++) {
			tmp = (node *)neighbor->pointers[i];
			tmp->parent = neighbor;
		}
	}

	/* In a leaf, append the keys and pointers of
	 * n to the neighbor.
	 * Set the neighbor's last pointer to pouint64_t to
	 * what had been n's right neighbor.
	 */

	else {
		for (i = neighbor_insertion_index, j = 0; j < n->num_keys; i++, j++) {
			neighbor->keys[i] = n->keys[j];
			neighbor->pointers[i] = n->pointers[j];
			neighbor->num_keys++;
		}
		neighbor->pointers[order - 1] = n->pointers[order - 1];
	}

	root = delete_entry(root, n->parent, k_prime, n);
	free(n->keys);
	free(n->pointers);
	free_node(n);
	return root;
}


/* Redistributes entries between two nodes when
 * one has become too small after deletion
 * but its neighbor is too big to append the
 * small node's entries without exceeding the
 * maximum
 */
node * redistribute_nodes(node * root, node * n, node * neighbor, uint64_t neighbor_index,
		uint64_t k_prime_index, uint64_t k_prime) {

	uint64_t i;
	node * tmp;

	/* Case: n has a neighbor to the left.
	 * Pull the neighbor's last key-pointer pair over
	 * from the neighbor's right end to n's left end.
	 */

	if (neighbor_index != -1) {
		if (!n->is_leaf)
			n->pointers[n->num_keys + 1] = n->pointers[n->num_keys];
		for (i = n->num_keys; i > 0; i--) {
			n->keys[i] = n->keys[i - 1];
			n->pointers[i] = n->pointers[i - 1];
		}
		if (!n->is_leaf) {
			n->pointers[0] = neighbor->pointers[neighbor->num_keys];
			tmp = (node *)n->pointers[0];
			tmp->parent = n;
			neighbor->pointers[neighbor->num_keys] = NULL;
			n->keys[0] = k_prime;
			n->parent->keys[k_prime_index] = neighbor->keys[neighbor->num_keys - 1];
		}
		else {
			n->pointers[0] = neighbor->pointers[neighbor->num_keys - 1];
			neighbor->pointers[neighbor->num_keys - 1] = NULL;
			n->keys[0] = neighbor->keys[neighbor->num_keys - 1];
			n->parent->keys[k_prime_index] = n->keys[0];
		}
	}

	/* Case: n is the leftmost child.
	 * Take a key-pointer pair from the neighbor to the right.
	 * Move the neighbor's leftmost key-pointer pair
	 * to n's rightmost position.
	 */

	else {
		if (n->is_leaf) {
			n->keys[n->num_keys] = neighbor->keys[0];
			n->pointers[n->num_keys] = neighbor->pointers[0];
			n->parent->keys[k_prime_index] = neighbor->keys[1];
		}
		else {
			n->keys[n->num_keys] = k_prime;
			n->pointers[n->num_keys + 1] = neighbor->pointers[0];
			tmp = (node *)n->pointers[n->num_keys + 1];
			tmp->parent = n;
			n->parent->keys[k_prime_index] = neighbor->keys[0];
		}
		for (i = 0; i < neighbor->num_keys - 1; i++) {
			neighbor->keys[i] = neighbor->keys[i + 1];
			neighbor->pointers[i] = neighbor->pointers[i + 1];
		}
		if (!n->is_leaf)
			neighbor->pointers[i] = neighbor->pointers[i + 1];
	}

	/* n now has one more key and one more pointer;
	 * the neighbor has one fewer of each.
	 */

	n->num_keys++;
	neighbor->num_keys--;

	return root;
}


/* Deletes an entry from the B+ tree.
 * Removes the record and its key and pointer
 * from the leaf, and then makes all appropriate
 * changes to preserve the B+ tree properties.
 */
node * delete_entry(node * root, node * n, uint64_t key, void * pointer) {

	uint64_t min_keys;
	node * neighbor;
	uint64_t neighbor_index;
	uint64_t k_prime_index, k_prime;
	uint64_t capacity;

	// Remove key and pointer from node.

	n = remove_entry_from_node(n, key, pointer);

	/* Case:  deletion from the root.
	*/

	if (n == root)
		return adjust_root(root);


	/* Case:  deletion from a node below the root.
	 * (Rest of function body.)
	 */

	/* Determine minimum allowable size of node,
	 * to be preserved after deletion.
	 */

	min_keys = n->is_leaf ? cut(order - 1) : cut(order) - 1;

	/* Case:  node stays at or above minimum.
	 * (The simple case.)
	 */

	if (n->num_keys >= min_keys)
		return root;

	/* Case:  node falls below minimum.
	 * Either coalescence or redistribution
	 * is needed.
	 */

	/* Find the appropriate neighbor node with which
	 * to coalesce.
	 * Also find the key (k_prime) in the parent
	 * between the pointer to node n and the pointer
	 * to the neighbor.
	 */

	neighbor_index = get_neighbor_index(n);
	k_prime_index = neighbor_index == -1 ? 0 : neighbor_index;
	k_prime = n->parent->keys[k_prime_index];
	neighbor = neighbor_index == -1 ? n->parent->pointers[1] :
		n->parent->pointers[neighbor_index];

	capacity = n->is_leaf ? order : order - 1;

	/* Coalescence. */

	if (neighbor->num_keys + n->num_keys < capacity)
		return coalesce_nodes(root, n, neighbor, neighbor_index, k_prime);

	/* Redistribution. */

	else
		return redistribute_nodes(root, n, neighbor, neighbor_index, k_prime_index, k_prime);
}



/* Master deletion function.
*/
node * delete(node * root, uint64_t key) {

	node * key_leaf = NULL;
	record * key_record = NULL;

	key_record = find(root, key, false, &key_leaf);

	/* CHANGE */

	if (key_record != NULL && key_leaf != NULL) {
		root = delete_entry(root, key_leaf, key, key_record);
		free_record(key_record);
	}
	return root;
}


void destroy_tree_nodes(node * root) {
	uint64_t i;
	if (root->is_leaf)
		for (i = 0; i < root->num_keys; i++)
			free(root->pointers[i]);
	else
		for (i = 0; i < root->num_keys + 1; i++)
			destroy_tree_nodes(root->pointers[i]);
	free(root->pointers);
	free(root->keys);
	free_node(root);
}


node * destroy_tree(node * root) {
	destroy_tree_nodes(root);
	return NULL;
}


// MAIN



#define N	16
#define MASK	((1 << (N - 1)) + (1 << (N - 1)) - 1)
#define LOW(x)	((unsigned)(x) & MASK)
#define HIGH(x)	LOW((x) >> N)
#define MUL(x, y, z)	{ int32_t l = (long)(x) * (long)(y); \
	(z)[0] = LOW(l); (z)[1] = HIGH(l); }
#define CARRY(x, y)	((int32_t)(x) + (long)(y) > MASK)
#define ADDEQU(x, y, z)	(z = CARRY(x, (y)), x = LOW(x + (y)))
#define X0	0x330E
#define X1	0xABCD
#define X2	0x1234
#define A0	0xE66D
#define A1	0xDEEC
#define A2	0x5
#define C	0xB
#define SET3(x, x0, x1, x2)	((x)[0] = (x0), (x)[1] = (x1), (x)[2] = (x2))
#define SETLOW(x, y, n) SET3(x, LOW((y)[n]), LOW((y)[(n)+1]), LOW((y)[(n)+2]))
#define SEED(x0, x1, x2) (SET3(x, x0, x1, x2), SET3(a, A0, A1, A2), c = C)
#define REST(v)	for (i = 0; i < 3; i++) { xsubi[i] = x[i]; x[i] = temp[i]; } \
			 return (v);
#define HI_BIT	(1L << (2 * N - 1))

static uint64_t x[3] = { X0, X1, X2 }, a[3] = { A0, A1, A2 }, c = C;
static void next(void);

uint64_t redisLrand48() {
	next();
	return (((uint64_t)x[2] << (N - 1)) + (x[1] >> 1));
}

void redisSrand48(int32_t seedval) {
	SEED(X0, LOW(seedval), HIGH(seedval));
}

static void next(void) {
	uint64_t p[2], q[2], r[2], carry0, carry1;

	MUL(a[0], x[0], p);
	ADDEQU(p[0], c, carry0);
	ADDEQU(p[1], carry0, carry1);
	MUL(a[0], x[1], q);
	ADDEQU(p[1], q[0], carry0);
	MUL(a[1], x[0], r);
	x[2] = LOW(carry0 + carry1 + CARRY(p[1], r[0]) + q[1] + r[1] +
			a[0] * x[2] + a[1] * x[1] + a[2] * x[0]);
	x[1] = LOW(p[1] + r[0]);
	x[0] = LOW(p[0]);
}

//int real_main(int argc, char ** argv) {
int main(int argc, char ** argv) {
	node * root;
	int numelems = NELEMENTS;
	int lookups = NLOOKUP;

	if(argc > 1) {
		numelems = atoi(argv[1]);
		if(numelems <= 0)
			numelems = NELEMENTS;
		//else 
		//	printf("WARNING: elements should be divisible by 1000\n");
	}

	if(argc > 2) {
		lookups = atoi(argv[2]);
		if(lookups <= 0)
			lookups = NLOOKUP;
	}

	printf("BTree Elements: %zu\n", (size_t)numelems);
	printf("BTree #Lookups: %zu\n", (size_t)lookups);
	root = NULL;
	verbose_output = false;

	order = 16;

	redisSrand48(0xcafebabe);

	struct element {
		uint64_t payload;
		uint64_t payload2;
	};

	struct element *elms = allocate((numelems / 2) * sizeof(struct element));
	struct element *elms2 = allocate((numelems / 2) * sizeof(struct element));

	printf("Btree Fanout: %zu\n", order);
	printf("Allocator: %zu bytes\n", allocator_stat);

	for (size_t i = 0; i < numelems; i += 2) {

		root = insert(root, i, (uint64_t)&elms[i/2]);
		root = insert(root, NELEMENTS - i - 1, (uint64_t)&elms2[i/2]);
	}

	//usleep(250);
	uint64_t sum = 0;

	//let's reduce simulaiton time
	//struct timeval start, end;
	//gettimeofday(&start, NULL);
#ifdef _OPENMP
#pragma omp parallel for
#endif
	for (size_t i = 0; i < NLOOKUP; i++) {
		size_t rdn = redisLrand48();
		record * r = find(root, rdn % numelems, false, NULL);
		if (r) {
			struct element *e = (struct element *)r->value;
			if (e) {
				sum += e->payload;
			}
		}

		r = find(root, ((rdn + 1) << 2) % numelems, false, NULL);
		if (r) {
			struct element *e = (struct element *)r->value;
			if (e) {
				sum += e->payload;
			}
		}
	}
	//gettimeofday(&end, NULL);
	//printf("got %zu matches in %zu seconds\n", sum, end.tv_sec - start.tv_sec);
	printf("Finished Execution\n");

    return EXIT_SUCCESS;
}
