#include "fs.h"

#define MY_MAX_PATH 256
#define MY_BLOCK_SIZE 16384
#define MY_MAX_BLOCKS 65536
#define MY_MAX_OPEN_FILES 1000
#define MY_MAX_FILE_SIZE MY_MAX_BLOCKS*MY_BLOCK_SIZE

/** @brief File control block */
struct my_fcb {
  uuid_t id; /**< File UUID */
  uid_t  uid; /**< User ID */
  gid_t  gid; /**< Group ID */
  mode_t mode; /**< Protection and file type */
  time_t atime; /**< Time of last access */
  time_t mtime; /**< Time of last modification */
  time_t ctime; /**< Time of last change to meta-data (status) */
  nlink_t nlink; /**< Number of hard links */
  off_t size; /**< File data size */
  uuid_t data; /**< Index block UUID */
};

/** @brief Index block */
struct my_index {
  uuid_t entries[MY_MAX_BLOCKS]; /**< Array of data block UUIDs */
};

/** @brief Directory header */
struct my_dir_header {
  int items; /**< Number of entries in the directory */
  int first_free; /**< Offset of the first unused entry, if no unused entry */
};

/** @bried Directory entry */
struct my_dir_entry {
  char name[MY_MAX_PATH]; /**< Entry name */
  uuid_t fcb_id; /**< UUID of the FCB for the entry file **/
  int next_free; /**< Offset of the next unused entry, if no next entry */
  char used; /**< Whether this entry is used */
};
