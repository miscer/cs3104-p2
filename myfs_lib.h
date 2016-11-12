#include "myfs.h"

#define MYFS_FIND_FOUND 0
#define MYFS_FIND_NO_DIR -1
#define MYFS_FIND_NO_FILE -2
#define MYFS_FIND_NO_ACCESS -3

/** @brief Struct used when iterating directories to hold iterator state */
struct my_dir_iter {
  int position; /**< Offset of the current entry */
  void* dir_data; /**< Pointer to raw directory data */
};

/** @brief Helper struct for storing user and group IDs for various functions */
struct my_user {
  uid_t  uid; /**< User ID */
  gid_t  gid; /**< Group ID */
};

/** @brief Keeps track of one open file */
struct my_open_file {
  uuid_t id; /**< ID of the FCB of the open file */
  char used; /**< Boolean variable for checking if this entry is used */
};

/**
 * @brief Reads an object from the database using the UUID as the key
 *
 * In case of an error the program is terminated and error is printed
 *
 * @param id UUID to be used as the key
 * @param buffer Buffer to store the loaded object
 * @param size Size of the buffer
 */
void read_db_object(uuid_t, void*, size_t);

/**
 * @brief Writes an object to the database using the UUID as the key
 *
 * In case of an error the program is terminated and error is printed
 *
 * @param id UUID to be used as the key
 * @param buffer Buffer containing the stored object
 * @param size Size of the buffer
 */
void write_db_object(uuid_t, void*, size_t);

/**
 * @brief Deletes an object from the database using the UUID as the key
 *
 * In case of an error the program is terminated and error is printed
 *
 * @param id UUID to be used as the key
 */
void delete_db_object(uuid_t);

/**
 * @brief Checks whether an object exists in the database using the UUID as the key
 * @param id UUID to be used as the key
 * @return 1 if the object exists, 0 otherwise
 */
char has_db_object(uuid_t);

/**
 * @brief Creates a new file and stores it in the database
 * @param mode File mode
 * @param user User and group IDs
 * @param file Pointer to a FCB where the created file will be stored
 */
void create_file(mode_t, struct my_user, struct my_fcb*);

/**
 * @brief Creates a new directory and stores it in the database
 * @param mode Directory mode
 * @param user User and group IDs
 * @param directory Pointer to a FCB where the created directory will be stored
 */
void create_directory(mode_t, struct my_user, struct my_fcb*);

/**
 * @brief Reads file FCB from the database into memory
 * @param id Database key
 * @param file Pointer to a FCB where the loaded file will be stored
 * @return 0 on success, -1 if not found, -2 on other error
 */
int read_file(uuid_t*, struct my_fcb*);

/**
 * @brief Finds a file by its path name in the directory tree
 *
 * Behaves the same way as find_dir_entry, but throws away the parent directory
 *
 * @param path Path to the file
 * @param user User and group IDs to check against when traversing the tree
 * @param file Pointer to a FCB where the found file will be stored
 * @return Same as find_dir_entry
 */
int find_file(const char*, struct my_user, struct my_fcb*);

/**
 * @brief Finds a file and its parent directory by its path name in the directory tree
 *
 * Traverses the directory tree to find the specified file. During traversal the
 * directory execute permission is checked and traversal can be stopped by
 * insufficient permissions. In this case MYFS_FIND_NO_ACCESS is returned.
 *
 * If a directory in the path (not the file) does not exist, MYFS_FIND_NO_DIR
 * is returned. If the file does not exist, MYFS_FIND_NO_FILE is returned.
 *
 * If all goes well, MYFS_FIND_FOUND is returned and the directory and file
 * is stored in dir and file. Parent directory is stored even if the file is not
 * found.
 *
 * @param path Path to the file
 * @param user User and group IDs to check against when traversing the tree
 * @param dir Pointer to a FCB where the parent directory will be stored
 * @param file Pointer to a FCB where the found file will be stored
 * @return Result of the traversal algorithm
 */
int find_dir_entry(const char*, struct my_user, struct my_fcb*, struct my_fcb*);

/**
 * @brief Writes the FCB into the database, replacing the previous version
 * @param fcb Pointer to the updated FCB, its ID is used as the database key
 */
void update_file(struct my_fcb*);

/**
 * @brief Removes the file and all its data from the database
 * @param fcb Pointer to the removed FCB, its ID is used as the database key
 */
void remove_file(struct my_fcb*);

/**
 * @brief Changes the size of the file, deleting or creating data blocks as needed
 *
 * All new blocks are filled with zeroes
 *
 * @param fcb Pointer to the updated FCB, its ID is used as the database key
 * @param size New size of the file
 */
void truncate_file(struct my_fcb*, size_t);

/**
 * @brief Reads a range of the file data into the buffer
 * @param fcb Pointer to the FCB of the read file
 * @param buffer Buffer for storing the read data
 * @param size Size of the buffer
 * @param offset Offset in the file to read data from
 */
void read_file_data(struct my_fcb*, void*, size_t, off_t);

/**
 * @brief Writes to a range of the file data from the buffer
 * @param fcb Pointer to the FCB of the updated file
 * @param buffer Buffer for reading the written data
 * @param size Size of the buffer
 * @param offset Offset in the file to write data to
 */
void write_file_data(struct my_fcb*, void*, size_t, off_t);

/**
 * @brief Adds an entry with the specified name to the directory
 * @param dir_fcb Pointer to the FCB of the directory
 * @param file_fcb Pointer to the FCB of the added file
 * @param name Name of the new directory entry
 */
void add_dir_entry(struct my_fcb*, struct my_fcb*, const char*);

/**
 * @brief Removes an entry with the specified name from the directory
 * @param dir_fcb Pointer to the FCB of the directory
 * @param name Name of the removed directory entry
 * @return 1 if the entry was in the directory, 0 otherwise
 */
int remove_dir_entry(struct my_fcb*, const char*);

/**
 * @brief Creates an iterator for directory entries
 *
 * The iterator can be then used with next_dir_entry and has to be passed
 * to clean_dir_iterator when it is no longer needed
 *
 * @param fcb Pointer to the FCB of the iterated directory
 * @param iterator Pointer to the previously allocated iterator
 */
void iterate_dir_entries(struct my_fcb*, struct my_dir_iter*);

/**
 * @brief Returns current directory entry and advances the iterator
 * @param iterator Pointer to the iterator
 * @return Pointer to an entry, or NULL if the iterator is at the end
 */
struct my_dir_entry* next_dir_entry(struct my_dir_iter*);

/**
 * @brief Deallocates memory used by the iterator
 * @param iterator Pointer to the iterator
 */
void clean_dir_iterator(struct my_dir_iter*);

/**
 * @brief Determines directory size
 * @param fcb Pointer to the FCB of the directory
 * @return Number of entries in the directory
 */
int get_directory_size(struct my_fcb*);

/**
 * @brief Adds a file to a directory and updates the number of links field
 *
 * This method has to be used for adding files to directories to maintain
 * the number of links pointing to the file value
 *
 * @param dir_fcb Pointer to the FCB of the parent directory
 * @param file_fcb Pointer to the FCB of the linked file
 * @param name Directory entry name
 */
void link_file(struct my_fcb*, struct my_fcb*, const char*);

/**
 * @brief Removes a file to a directory and updates the number of links field
 *
 * This method has to be used for removing files from directories to maintain
 * the number of links pointing to the file value.
 *
 * If the file has no other links pointing to it and it is not open, it is
 * deleted from the database.
 *
 * @param dir_fcb Pointer to the FCB of the parent directory
 * @param file_fcb Pointer to the FCB of the linked file
 * @param name Directory entry name
 */
void unlink_file(struct my_fcb*, struct my_fcb*, const char*);

/**
 * @brief Splits a path string into the first component and the rest
 *
 * Path component is anything between path separators ('/').
 *
 * This function uses strsep internally and the behaviour is the same. The
 * original string is modified so that it is split into two strings by replacing
 * a '/' character with '\0'.
 *
 * The passed string pointer is then changed to point to the rest of the path
 * and pointer to the first component is returned.
 *
 * If the path is empty, NULL is returned. If there are no other path components
 * in the rest of the path, the passed pointer is changed to NULL.
 *
 * @param Pointer to the string containing the path
 * @return String containing the first component of the path
 */
char* path_split(char**);

/**
 * @brief Returns the last component (file name) of the path
 *
 * The passed string is modified and should not be used again.
 *
 * @param Path string
 * @return String containing the last component of the path
 */
char* path_file_name(char*);

/**
 * @brief Determines whether the FCB is a directory
 * @param Pointer to the FCB
 * @return 1 if it is a directory, 0 otherwise
 */
char is_directory(struct my_fcb*);

/**
 * @brief Determines whether the FCB is a regular file
 * @param Pointer to the FCB
 * @return 1 if it is a file, 0 otherwise
 */
char is_file(struct my_fcb*);

/**
 * @brief Reads the user and group IDs from FUSE context
 * @return User struct with user and group IDs
 */
struct my_user get_context_user();

/**
 * @brief Determines whether the user can read the file based on its mode
 * @param fcb Pointer to the FCB of the file
 * @param user User struct containing user and group ID
 * @return 1 if the user can read, 0 otherwise
 */
char can_read(struct my_fcb*, struct my_user);

/**
 * @brief Determines whether the user can write the file based on its mode
 * @param fcb Pointer to the FCB of the file
 * @param user User struct containing user and group ID
 * @return 1 if the user can write, 0 otherwise
 */
char can_write(struct my_fcb*, struct my_user);

/**
 * @brief Determines whether the user can execute the file based on its mode
 * @param fcb Pointer to the FCB of the file
 * @param user User struct containing user and group ID
 * @return 1 if the user can execute, 0 otherwise
 */
char can_execute(struct my_fcb*, struct my_user);

/**
 * @brief Checks permissions based on flags used in an open call
 * @param fcb Pointer to the FCB of the file
 * @param user User struct containing user and group ID
 * @param flags Flags from the open call
 * @return 1 if the user can access the file as requested, 0 otherwise
 */
char check_open_flags(struct my_fcb*, struct my_user, int);

/**
 * @brief Finds a file by its handle in the open file table
 * @param fcb Pointer to a FCB where the found open file
 * @return 0 if there is a file with this handle, -1 otherwise
 */
int get_open_file(int, struct my_fcb*);

/**
 * @brief Adds the file to the open file table and returns its handle
 * @param fcb Pointer to the FCB of the file
 * @return File handle to retrieve the file later
 */
int add_open_file(struct my_fcb*);

/**
 * @brief Removes the file from the open file table based on its handle
 *
 * After the file is closed, there are no links to it and it is not open with
 * other handles, it is removed from the database.
 *
 * @param handle File handle
 * @return 1 if there was a file with the specified handle, 0 otherwise
 */
int remove_open_file(int);

/**
 * @brief Checks whether the file is open
 * @param fcb Pointer to the FCB of the file
 * @return 1 if the file is open, 0 otherwise
 */
char is_file_open(struct my_fcb*);

/**
 * @brief Rounds size up to a multiple of the specified number
 * @param Number to round up
 * @param Number to round up to
 * @return Rounded up number
 */
size_t size_round_up_to(size_t num, size_t up_to);

/**
 * @brief Rounds size down to a multiple of the specified number
 * @param Number to round down
 * @param Number to round down to
 * @return Rounded down number
 */
size_t size_round_down_to(size_t num, size_t up_to);

/**
 * @brief Calculates the number of data blocks needed to store the specified size
 * @param File size
 * @return Number of blocks
 */
int get_num_blocks(size_t size);

/**
 * @brief Calculates the indexes of the first and last block for data range
 *
 * To write or read a range of the data specified by its size and offset, blocks
 * in the returned range need to be accessed.
 *
 * @param Data range size
 * @param Data range offset
 * @param Pointer to an integer where the first block index will be stored
 * @param Pointer to an integer where the last block index will be stored
 */
void get_block_indexes(size_t size, off_t offset, int* first, int* last);
