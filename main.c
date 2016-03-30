/*
    longest-word : simple solution using a trie and a list for the well-known 'longest concatenated word' problem.
    Copyright (C) 2016  Laurent Parenteau

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

// data structures
#define NBCHAR 26
typedef struct trie {
	char word_end;
	struct trie *children[NBCHAR];
} trie;

typedef struct missing_list {
	char *missing;
	struct missing_list *next;
} missing_list;

typedef struct pending_list {
	char *word;
	missing_list *missing;
	struct pending_list *next;
} pending_list;

// helpers for trie
missing_list *insert(trie *node, char *word, size_t offset, size_t len, missing_list *missing);
int is_in_trie(trie *node, char *word, size_t len);
void free_trie(trie *node);
#define INDEX(x) ((x) - 'a')

// helpers for pending_list
missing_list *push_to_missing(missing_list *list, char *missing);
pending_list *push_to_pending(pending_list *list, char *word, missing_list *missing);

// helpers for arguments
static FILE * parse_arguments(int argc, char **argv);
static void usage();

// for better branch predictions
#ifdef __GNUC__
#define likely(x)       __builtin_expect(!!(x), 1)
#define unlikely(x)     __builtin_expect(!!(x), 0)
#else
#define likely(x)       (x)
#define unlikely(x)     (x)
#endif

// globals, to save on arguments passing
pending_list *pending;
trie *root;

int main(int argc, char **argv)
{
	FILE *file;

	if (NULL == (file = parse_arguments(argc, argv))) {
		usage();
		return 1;
	}

	char *line = NULL;
	size_t len = 0;
	ssize_t read;

	root = (trie *)calloc(1, sizeof(trie));
	pending = NULL;
	
	// read each line
	while ((read = getline(&line, &len, file)) != -1) {
		// if empty line (ie. \r\n), skip it
		if (unlikely(read <= 2)) {
			continue;
		}

		char has_prefix = 1;
		// if that slot is empty, then no prefix at all
		if (unlikely(NULL == root->children[INDEX(line[0])])) {
			has_prefix = 0;
		}

		// insert the word in the trie.  Again, skip '\r\n' at end of word.
		missing_list *missing = NULL;
		missing = insert(root, line, 0, read-2, missing);
		if (missing) {
			pending = push_to_pending(pending, strdup(line), missing);
		}
	}

	char *longest = NULL;
	size_t longest_len = 0;
	char *oldlongest = NULL;
	size_t oldlongest_len = 0;
	unsigned int total = 0;

	// Ok, check all the words in the pending list, to see if we can match something.
	while (NULL != pending) {
		int matched = 0;
		while (NULL != pending->missing) {
			// If we can match the missing part with words in the trie, then this is a concatenated word.
			if ((0 == matched) && (is_in_trie(root, pending->missing->missing, strlen(pending->missing->missing)) == 1)) {
				size_t len = strlen(pending->word);
				// check new longest word
				if (len > longest_len) {
					if (oldlongest) {
						free(oldlongest);
					}
					oldlongest = longest;
					oldlongest_len = longest_len;
					longest = strdup(pending->word);
					longest_len = len;
				// otherwise check 2nd longest word
				} else if (len > oldlongest_len) {
					if (oldlongest) {
						free(oldlongest);
					}
					oldlongest = strdup(pending->word);
					oldlongest_len = len;
				}
				total++;
				matched = 1;
			}
			// get the next missing part
			missing_list *next_miss = pending->missing->next;
			free(pending->missing->missing);
			free(pending->missing);
			pending->missing = next_miss;
		}

		// get next pending word
		pending_list *next = pending->next;
		free(pending->word);
		free(pending);
		pending = next;
	}
	
	printf("Longest concatenated word is : %s", longest?longest:"NULL");
	printf("2nd longest concatenated word is : %s", oldlongest?oldlongest:"NULL");
	printf("There are %d concatenated words in the file.\n", total);

	// cleanup ;
	free_trie(root);
	if (line) {
		free(line);
	}
	if (longest) {
		free(longest);
	}
	if (oldlongest) {
		free(oldlongest);
	}

	fclose(file);

	return 0;
}

/*
 * Insert a word in the trie.
 *
 * Return a list if missing part of the word, that we'll have to look later to verify if that was a concatenaded word.
 */
missing_list *insert(trie *node, char *word, size_t offset, size_t len, missing_list *missing)
{
	// If there's more data, keep inserting.  Else we'll mark this node as a end of word.
	if (likely(len - offset > 0)) {
		// If we are currently at the end of a known word, then the remaining need to be added to the missing list.
		if (node->word_end) {
			missing = push_to_missing(missing, strndup(word+offset, len-offset));
		}
		// Allocate a new children if we've never been down that path before.
		if (NULL == node->children[INDEX(word[offset])]) {
			node->children[INDEX(word[offset])] = (trie *)calloc(1, sizeof(trie));
		}

		missing = insert(node->children[INDEX(word[offset])], word, offset+1, len, missing);
	} else {
		node->word_end = 1;
	}

	return missing;
}

/*
 * Check if a word is in the trie.
 *
 * Return 1 if it is, 0 otherwise.
 */
int is_in_trie(trie *node, char *word, size_t len)
{
	// missing a mode, can't be a match down that road.
	if (NULL == node) {
		return 0;
	}

	// if this was the last character of the word, than a match exist if this is the end of a known word.
	if (len == 0) {
		return node->word_end;
	}

	// if there's still character to check for, it can either match a word on the remaining of that part of the trie,
	// OR, if this is a word boundary, then we can check for a match restarting at the root as well.
	return is_in_trie(node->children[INDEX(word[0])], word+1, len-1) || (node->word_end && is_in_trie(root, word, len));
}

/*
 * Cleanup a trie.
 */
void free_trie(trie *node)
{
	for (int i = 0; i < NBCHAR; i++) {
		if (node->children[i]) {
			free_trie(node->children[i]);
		}
	}
	free(node);
}

/*
 * Push a missing part of word into the list of missing part (for that word).
 *
 * Return the new head of the list.
 */
missing_list *push_to_missing(missing_list *list, char *missing)
{
	missing_list *new = (missing_list *)malloc(sizeof(missing_list));

	new->next = list;
	new->missing = missing;

	return new;
}

/*
 * Push a word which we have a prefix but missing the last part of the word on a list to check later, once the trie is complete.
 *
 * Return the new head of the list.
 */
pending_list *push_to_pending(pending_list *list, char *word, missing_list *missing)
{
	pending_list *new = (pending_list *)malloc(sizeof(pending_list));

	new->next = list;
	new->word = word;
	new->missing = missing;

	return new;
}

/*
 * Parse arguments from command line.
 *
 * Return valid and opened FILE pointer if a filename was supplied and successfully opened.
 * Return NULL otherwise.
 */
static FILE *parse_arguments(int argc, char **argv)
{
	FILE *file = NULL;
	if (2 == argc) {
		file = fopen(argv[1], "r");
		if (NULL == file) {
			printf("Unable to open file %s\n", argv[1]);
			perror(NULL);
		}
        }

        return file;
}

/*
 * Print program usage.
 */
static void usage()
{
        printf("arguments : \n");
        printf("  <file>     file with sorted words, one per line\n");
}
