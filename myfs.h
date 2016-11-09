#include "fs.h"

#define MY_MAX_PATH 256

struct my_fcb {
  uuid_t id;    /* file ID */
  uid_t  uid;   /* user */
  gid_t  gid;   /* group */
  mode_t mode;  /* protection */
  time_t atime; /* time of last access */
  time_t mtime; /* time of last modification */
  time_t ctime; /* time of last change to meta-data (status) */
  nlink_t nlink; /* number of hard links */
  off_t size;   /* size */
  uuid_t data;  /* data */
};

struct my_dir_header {
  int items;
  int first_free;
};

struct my_dir_entry {
  char name[MY_MAX_PATH];
  uuid_t fcb_id;
  int next_free;
  char used;
};
