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
  file_fcb->nlink = 0;

  uuid_generate(file_fcb->id);

  puts("Creating file...");
  print_id(&(file_fcb->id));
  puts("\n");

  int rc = unqlite_kv_store(pDb, &(file_fcb->id), KEY_SIZE, file_fcb, sizeof(struct my_fcb));

  if (rc != UNQLITE_OK) {
    error_handler(rc);
  }
}

int read_file(uuid_t *id, struct my_fcb* file_fcb) {
  puts("Reading file...");
  print_id(id);
  puts("\n");

  unqlite_int64 size = sizeof(struct my_fcb);
  int rc = unqlite_kv_fetch(pDb, id, KEY_SIZE, file_fcb, &size);

  if (rc == UNQLITE_OK) {
    return 0;

  } else if (rc == UNQLITE_NOTFOUND) {
    puts("File not found");
    return -1;

  } else {
    error_handler(rc);
    return -2;
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

void remove_file(struct my_fcb* file_fcb) {
  puts("Removing file...");
  print_id(&(file_fcb->id));
  puts("\n");

  int rc = unqlite_kv_delete(pDb, &(file_fcb->id), KEY_SIZE);
  if (rc != UNQLITE_OK) error_handler(rc);

  unqlite_kv_delete(pDb, &(file_fcb->data), KEY_SIZE);
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
  size_t data_size = dir_fcb->size;
  size_t max_size = data_size + sizeof(struct my_dir_entry);

  void* dir_data = malloc(max_size);
  read_file_data(*dir_fcb, dir_data);

  struct my_dir_header* dir_header = dir_data;

  struct my_dir_entry* free_entry = NULL;

  for (int n = 0; n < dir_header->items; n++) {
    struct my_dir_entry* dir_entry = dir_data + sizeof(struct my_dir_header) +
      n * sizeof(struct my_dir_entry);

    if (!dir_entry->used) {
      free_entry = dir_entry;
      break;
    }
  }

  if (free_entry == NULL) {
    data_size = max_size;
    free_entry = dir_data + sizeof(struct my_dir_header) +
      dir_header->items * sizeof(struct my_dir_entry);
    dir_header->items++;
  }

  strncpy(free_entry->name, name, MY_MAX_PATH - 1);
  uuid_copy(free_entry->fcb_id, file_fcb->id);
  free_entry->used = 1;

  write_file_data(dir_fcb, dir_data, data_size);
  free(dir_data);
}

int remove_dir_entry(struct my_fcb* dir_fcb, const char* name) {
  void* dir_data = malloc(dir_fcb->size);
  read_file_data(*dir_fcb, dir_data);

  struct my_dir_header* dir_header = dir_data;

  struct my_dir_entry* dir_entry;
  char found = 0;

  for (int n = 0; n < dir_header->items; n++) {
    dir_entry = dir_data + sizeof(struct my_dir_header) +
      n * sizeof(struct my_dir_entry);

    if (!dir_entry->used) continue;

    if (strcmp(dir_entry->name, name) == 0) {
      found = 1;
      break;
    }
  }

  if (found) {
    memset(dir_entry, 0, sizeof(struct my_dir_entry));
    write_file_data(dir_fcb, dir_data, dir_fcb->size);

    return 0;
  } else {
    return -1;
  }
}

void iterate_dir_entries(struct my_fcb* dir_fcb, struct my_dir_iter* iter) {
  iter->position = 0;
  iter->dir_data = malloc(dir_fcb->size);

  read_file_data(*dir_fcb, iter->dir_data);
}

struct my_dir_entry* next_dir_entry(struct my_dir_iter* iter) {
  struct my_dir_header* dir_header = iter->dir_data;

  while (iter->position < dir_header->items) {
    struct my_dir_entry* entry =
      iter->dir_data + sizeof(struct my_dir_header) +
      iter->position * sizeof(struct my_dir_entry);

    iter->position++;

    if (entry->used) {
      return entry;
    }
  }

  return NULL;
}

void clean_dir_iterator(struct my_dir_iter* iter) {
  free(iter->dir_data);
}

int get_directory_size(struct my_fcb* dir_fcb) {
  struct my_dir_header* dir_header = malloc(dir_fcb->size);
  read_file_data(*dir_fcb, dir_header);

  int size = dir_header->items;
  free(dir_header);

  return size;
}

void link_file(struct my_fcb* dir_fcb, struct my_fcb* file_fcb, const char* name) {
  add_dir_entry(dir_fcb, file_fcb, name);

  file_fcb->nlink++;
  update_file(*file_fcb);
}

void unlink_file(struct my_fcb* dir_fcb, struct my_fcb* file_fcb, const char* name) {
  remove_dir_entry(dir_fcb, name);

  if (file_fcb->nlink > 1) {
    file_fcb->nlink--;
    update_file(*file_fcb);

  } else {
    remove_file(file_fcb);
  }
}

int find_file(const char* path, struct my_fcb* file_fcb) {
  struct my_fcb dir_fcb;
  return find_dir_entry(path, &dir_fcb, file_fcb);
}

int find_dir_entry(const char* const_path, struct my_fcb* dir_fcb, struct my_fcb* file_fcb) {
  struct my_fcb root_dir;
  read_file(&(root_object.id), &root_dir);

  memcpy(file_fcb, &root_dir, sizeof(struct my_fcb));

  char* full_path = strdup(const_path);
  char* path = full_path;

  char* entry_name = path_split(&path);

  while (entry_name != NULL) {
    if (!is_directory(file_fcb)) {
      free(full_path);
      return MYFS_FIND_NO_DIR;
    }

    memcpy(dir_fcb, file_fcb, sizeof(struct my_fcb));

    struct my_dir_iter iter;
    iterate_dir_entries(dir_fcb, &iter);

    struct my_dir_entry* entry;
    char found = 0;

    while ((entry = next_dir_entry(&iter)) != NULL) {
      if (strcmp(entry_name, entry->name) == 0) {
        found = 1;
        break;
      }
    }

    if (found) {
      read_file(&(entry->fcb_id), file_fcb);
      entry_name = path_split(&path);
    }

    clean_dir_iterator(&iter);

    if (!found) {
      free(full_path);
      return (path == NULL) ? MYFS_FIND_NO_FILE : MYFS_FIND_NO_DIR;
    }
  }

  free(path);
  return MYFS_FIND_FOUND;
}

char* path_split(char** path) {
  char* head = strsep(path, "/");

  if (head == NULL) {
    return NULL;
  } else if (*head == '\0') {
    if (path != NULL) {
      return path_split(path);
    } else {
      return NULL;
    }
  } else {
    return head;
  }
}

char* path_file_name(char* path) {
  char* file_name = path_split(&path);

  while (path != NULL) {
    char* next_file_name = path_split(&path);

    if (next_file_name != NULL) {
      file_name = next_file_name;
    }
  }

  return file_name;
}

char is_directory(struct my_fcb* fcb) {
  return (fcb->mode & S_IFDIR) == S_IFDIR;
}
