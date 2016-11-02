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

void create_file(mode_t mode, struct my_fcb* file_fcb) {
  file_fcb->uid = getuid();
  file_fcb->gid = getgid();
  file_fcb->mode = S_IFREG|mode;
  file_fcb->mtime = time(0);
  file_fcb->ctime = time(0);
  file_fcb->size = 0;

  uuid_generate(file_fcb->id);

  int rc = unqlite_kv_store(pDb, &(file_fcb->id), KEY_SIZE, file_fcb, sizeof(struct my_fcb));

  if (rc != UNQLITE_OK) {
    error_handler(rc);
  }
}

void read_file(uuid_t id, struct my_fcb* file_fcb) {
  unqlite_int64 size = sizeof(struct my_fcb);
  int rc = unqlite_kv_fetch(pDb, id, KEY_SIZE, file_fcb, &size);

  if (rc != UNQLITE_OK) {
    error_handler(rc);
  }
}

void update_file(struct my_fcb file_fcb) {
  int rc = unqlite_kv_store(pDb, &(file_fcb.id), KEY_SIZE, &file_fcb, sizeof(struct my_fcb));

  if (rc != UNQLITE_OK) {
    error_handler(rc);
  }
}
