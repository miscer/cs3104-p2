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
void update_file(struct my_fcb);
void remove_file(struct my_fcb*);
void truncate_file(struct my_fcb*, size_t);
void read_file_data(struct my_fcb, void*, size_t, off_t);
void write_file_data(struct my_fcb*, void*, size_t, off_t);
void add_dir_entry(struct my_fcb*, struct my_fcb*, const char*);
int remove_dir_entry(struct my_fcb*, const char*);
void iterate_dir_entries(struct my_fcb*, struct my_dir_iter*);
struct my_dir_entry* next_dir_entry(struct my_dir_iter*);
void clean_dir_iterator(struct my_dir_iter*);
int get_directory_size(struct my_fcb*);
void link_file(struct my_fcb*, struct my_fcb*, const char*);
void unlink_file(struct my_fcb*, struct my_fcb*, const char*);
char* path_split(char**);
char* path_file_name(char*);
char is_directory(struct my_fcb*);
char is_file(struct my_fcb*);
struct my_user get_context_user();
char can_read(struct my_fcb*, struct my_user);
char can_write(struct my_fcb*, struct my_user);
char can_execute(struct my_fcb*, struct my_user);
char check_open_flags(struct my_fcb*, struct my_user, int);
int get_open_file(int, struct my_fcb*);
int add_open_file(struct my_fcb*);
int remove_open_file(int);
char is_file_open(struct my_fcb*);
