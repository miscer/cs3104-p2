#include "myfs_lib.h"

void create_directory(mode_t mode, struct my_fcb *dir_fcb) {
  dir_fcb->uid = getuid();
  dir_fcb->gid = getgid();
  dir_fcb->mode = S_IFDIR|mode;
  dir_fcb->mtime = time(0);
  dir_fcb->ctime = time(0);
  dir_fcb->size = 0;

  uuid_generate(dir_fcb->id);
  
  int rc = unqlite_kv_store(pDb, &(dir_fcb->id), KEY_SIZE, dir_fcb, sizeof(struct my_fcb));

  if (rc != UNQLITE_OK) {
    error_handler(rc);
  }
}
