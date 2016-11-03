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

  struct my_dir_header dir_header = {0};
  write_file_data(dir_fcb, &dir_header, sizeof(dir_header));
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

void add_dir_entry(struct my_fcb* dir_fcb, struct my_fcb* file_fcb, const char* name) {
  size_t size = dir_fcb->size + sizeof(struct my_dir_entry);

  void* dir_data = malloc(size);
  read_file_data(*dir_fcb, dir_data);

  struct my_dir_header* dir_header = dir_data;
  struct my_dir_entry* dir_entry = dir_data + sizeof(struct my_dir_header) +
    dir_header->items * sizeof(struct my_dir_entry);

  strncpy(dir_entry->name, name, MY_MAX_PATH - 1);
  uuid_copy(dir_entry->fcb_id, file_fcb->id);

  dir_header->items++;

  write_file_data(dir_fcb, dir_data, size);
  free(dir_data);
}

void remove_dir_entry(struct my_fcb* dir_fcb, struct my_fcb* file_fcb) {

}

void iterate_dir_entries(struct my_fcb* dir_fcb, struct my_dir_iter* iter) {
  iter->position = 0;
  iter->dir_data = malloc(dir_fcb->size);

  read_file_data(*dir_fcb, iter->dir_data);
}

struct my_dir_entry* next_dir_entry(struct my_dir_iter* iter) {
  struct my_dir_header* dir_header = iter->dir_data;

  if (iter->position < dir_header->items) {
    struct my_dir_entry* entry =
      iter->dir_data + sizeof(struct my_dir_header) +
      iter->position * sizeof(struct my_dir_entry);

    iter->position++;

    return entry;
  } else {
    free(iter->dir_data);
    return NULL;
  }
}

int find_file(const char* path, struct my_fcb* file_fcb) {
  struct my_fcb dir_fcb;
  return find_dir_entry(path, &dir_fcb, file_fcb);
}

int find_dir_entry(const char* path, struct my_fcb* dir_fcb, struct my_fcb* file_fcb) {
  char* filename = path + 1;

  struct my_fcb root_dir;
  read_file(&(root_object.id), &root_dir);

  struct my_dir_iter iter;
  iterate_dir_entries(&root_dir, &iter);

  struct my_dir_entry* entry;
  char found = 0;

  while ((entry = next_dir_entry(&iter)) != NULL) {
    if (strcmp(entry->name, filename) == 0) {
      found = 1;
      break;
    }
  }

  if (found) {
    read_file(&(entry->fcb_id), file_fcb);
    return MYFS_FIND_FOUND;
  } else {
    return MYFS_FIND_NO_FILE;
  }
}
