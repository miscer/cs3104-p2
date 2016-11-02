#include "myfs_lib.h"

void create_directory(mode_t mode, struct my_fcb *dir_fcb) {
  dir_fcb->uid = getuid();
  dir_fcb->gid = getgid();
  dir_fcb->mode = S_IFDIR|mode;
  dir_fcb->mtime = time(0);
  dir_fcb->ctime = time(0);
  dir_fcb->size = 0;

  uuid_generate(dir_fcb->id);

  puts("Creating directory...");
  print_id(&(dir_fcb->id));
  puts("\n");

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

  puts("Creating file...");
  print_id(&(file_fcb->id));
  puts("\n");

  int rc = unqlite_kv_store(pDb, &(file_fcb->id), KEY_SIZE, file_fcb, sizeof(struct my_fcb));

  if (rc != UNQLITE_OK) {
    error_handler(rc);
  }
}

void read_file(uuid_t *id, struct my_fcb* file_fcb) {
  puts("Reading file...");
  print_id(id);
  puts("\n");

  unqlite_int64 size = sizeof(struct my_fcb);
  int rc = unqlite_kv_fetch(pDb, id, KEY_SIZE, file_fcb, &size);

  if (rc == UNQLITE_NOTFOUND) {
    puts("File not found");
  } else if (rc != UNQLITE_OK) {
    error_handler(rc);
  }
}

void update_file(struct my_fcb file_fcb) {
  puts("Updating file...");
  print_id(&(file_fcb.id));
  puts("\n");

  int rc = unqlite_kv_store(pDb, &(file_fcb.id), KEY_SIZE, &file_fcb, sizeof(struct my_fcb));

  if (rc != UNQLITE_OK) {
    error_handler(rc);
  }
}

void read_file_data(struct my_fcb file_fcb, void* buffer) {
  puts("Reading file data...");
  print_id(&(file_fcb.id));
  puts("\n");

  unqlite_int64 size = file_fcb.size;
  int rc = unqlite_kv_fetch(pDb, &(file_fcb.data), KEY_SIZE, buffer, &size);

  if (rc == UNQLITE_NOTFOUND) {
    puts("File data not found");
  } else if (rc != UNQLITE_OK) {
    error_handler(rc);
  }
}

void write_file_data(struct my_fcb* file_fcb, void* buffer, size_t size) {
  puts("Writing file data...");
  print_id(&(file_fcb->id));
  puts("\n");

  file_fcb->size = size;
  uuid_generate(file_fcb->data);
  update_file(*file_fcb);

  int rc = unqlite_kv_store(pDb, &(file_fcb->data), KEY_SIZE, buffer, file_fcb->size);

  if (rc != UNQLITE_OK) {
    error_handler(rc);
  }
}
