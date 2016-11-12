#include "myfs_lib.h"

/**
 * @var Open file table
 * Array indexes are the file handles used to identify open files
 */
static struct my_open_file open_files[MY_MAX_OPEN_FILES];

void read_db_object(uuid_t key, void* buffer, size_t size) {
  // separate variable for size is required because unqlite will store
  // the read size in it
  unqlite_int64 unqlite_size = size;
  int rc = unqlite_kv_fetch(pDb, key, KEY_SIZE, buffer, &unqlite_size);
  error_handler(rc);
}

void write_db_object(uuid_t key, void* buffer, size_t size) {
  int rc = unqlite_kv_store(pDb, key, KEY_SIZE, buffer, size);
  error_handler(rc);
}

void delete_db_object(uuid_t key) {
  int rc = unqlite_kv_delete(pDb, key, KEY_SIZE);
  error_handler(rc);
}

char has_db_object(uuid_t key) {
  unqlite_int64 unqlite_size;
  // NULL is used as the buffer to prevent actually reading the object
  int rc = unqlite_kv_fetch(pDb, key, KEY_SIZE, NULL, &unqlite_size);

  if (rc == UNQLITE_OK) {
    return 1;
  } else if (rc == UNQLITE_NOTFOUND) {
    return 0;
  } else {
    error_handler(rc);
  }
}

void create_directory(mode_t mode, struct my_user user, struct my_fcb *dir_fcb) {
  dir_fcb->uid = user.uid;
  dir_fcb->gid = user.gid;
  dir_fcb->mode = S_IFDIR|mode;
  dir_fcb->mtime = time(0);
  dir_fcb->ctime = time(0);
  dir_fcb->size = 0;
  dir_fcb->nlink = 0;

  // create uuids for the FCB and index block
  uuid_generate(dir_fcb->id);
  uuid_generate(dir_fcb->data);

  puts("Creating directory...");
  print_id(&(dir_fcb->id));
  puts("\n");

  // write the FCB to the database
  write_db_object(dir_fcb->id, dir_fcb, sizeof(struct my_fcb));

  // create an empty index block and write it to the database
  struct my_index index_block;
  write_db_object(dir_fcb->data, &index_block, sizeof(index_block));

  // create an empty directory header and write it to the database
  struct my_dir_header dir_header = {0, -1};
  write_file_data(dir_fcb, &dir_header, sizeof(dir_header), 0);
}

void create_file(mode_t mode, struct my_user user, struct my_fcb* file_fcb) {
  file_fcb->uid = user.uid;
  file_fcb->gid = user.gid;
  file_fcb->mode = S_IFREG|mode;
  file_fcb->mtime = time(0);
  file_fcb->ctime = time(0);
  file_fcb->size = 0;
  file_fcb->nlink = 0;

  // create uuids for the FCB and index block
  uuid_generate(file_fcb->id);
  uuid_generate(file_fcb->data);

  puts("Creating file...");
  print_id(&(file_fcb->id));
  puts("\n");

  // write the FCB to the database
  write_db_object(file_fcb->id, file_fcb, sizeof(struct my_fcb));

  // create an empty index block and write it to the database
  struct my_index index_block;
  write_db_object(file_fcb->data, &index_block, sizeof(index_block));
}

int read_file(uuid_t *id, struct my_fcb* file_fcb) {
  puts("Reading file...");
  print_id(id);
  puts("\n");

  if (has_db_object(*id)) {
    read_db_object(*id, file_fcb, sizeof(struct my_fcb));
    return 0;

  } else {
    puts("File not found");
    return -1;
  }
}

void update_file(struct my_fcb* file_fcb) {
  puts("Updating file...");
  print_id(&(file_fcb->id));
  puts("\n");

  write_db_object(file_fcb->id, file_fcb, sizeof(struct my_fcb));
}

size_t size_round_up_to(size_t num, size_t up_to) {
  size_t rem = num % up_to;

  if (rem > 0) {
    // add to the number so that it is a multiple of up_to
    return num + (up_to - rem);
  } else {
    return num;
  }
}

size_t size_round_down_to(size_t num, size_t up_to) {
  size_t rem = num % up_to;

  // subtract from the number so that it is a multiple of up_to
  return num - rem;
}

int get_num_blocks(size_t size) {
  return size_round_up_to(size, MY_BLOCK_SIZE) / MY_BLOCK_SIZE;
}

void get_block_indexes(size_t size, off_t offset, int* first, int* last) {
  *first = size_round_down_to(offset, MY_BLOCK_SIZE) / MY_BLOCK_SIZE;
  *last = size_round_up_to(offset + size, MY_BLOCK_SIZE) / MY_BLOCK_SIZE - 1;
}

void remove_file(struct my_fcb* file_fcb) {
  puts("Removing file...");
  print_id(&(file_fcb->id));
  puts("\n");

  // read the index block for the file
  struct my_index index_block;
  read_db_object(file_fcb->data, &index_block, sizeof(index_block));

  // get the number of data blocks used by the file data
  int num_blocks = get_num_blocks(file_fcb->size);

  // delete all data blocks
  for (int block = 0; block < num_blocks; block++) {
    delete_db_object(index_block.entries[block]);
  }

  // delete the index block and FCB
  delete_db_object(file_fcb->data);
  delete_db_object(file_fcb->id);
}

void truncate_file(struct my_fcb* file_fcb, size_t size) {
  // read the index block for the file
  struct my_index index_block;
  read_db_object(file_fcb->data, &index_block, sizeof(index_block));

  /** @var Number of data blocks currently used by the file */
  int old_num_blocks = get_num_blocks(file_fcb->size);

  /** @var Number of data blocks the file needs to have */
  int new_num_blocks = get_num_blocks(size);

  if (new_num_blocks > old_num_blocks) {
    // we need to create some blocks at the end of the file

    /** @var Pointer to a data block filled with zeroes */
    void* empty_block = calloc(1, MY_BLOCK_SIZE);

    // go through all data blocks we need to create
    for (int block = old_num_blocks; block < new_num_blocks; block++) {
      // create UUID for the data block and write an empty data block into the database
      uuid_generate(index_block.entries[block]);
      write_db_object(index_block.entries[block], empty_block, MY_BLOCK_SIZE);
    }

    free(empty_block);

    // save changes made in the index block to the database
    write_db_object(file_fcb->data, &index_block, sizeof(index_block));

  } else if (new_num_blocks < old_num_blocks) {
    // we need to remove some blocks at the end of the file

    // go through all data blocks that need to be removed
    for (int block = new_num_blocks; block < old_num_blocks; block++) {
      delete_db_object(index_block.entries[block]);
    }
  }

  // finally update file size
  file_fcb->size = size;
  update_file(file_fcb);
}

void read_block_to_buffer(uuid_t id, int block_num, void* buffer, size_t size, off_t offset) {
  // read the data from the data block into memory
  void* block_data = malloc(MY_BLOCK_SIZE);
  read_db_object(id, block_data, MY_BLOCK_SIZE);

  /** @var Offset in the file of the first byte of the block */
  off_t block_start = block_num * MY_BLOCK_SIZE;
  /** @var Offset in the file of the last byte of the block */
  off_t block_end = block_start + MY_BLOCK_SIZE - 1;

  /** @var Offset in the file of the first byte of the read data */
  off_t data_start = offset;
  /** @var Offset in the file of the last byte of the read data */
  off_t data_end = offset + size - 1;

  // requested data range is fully contained in the block
  if (block_start <= data_start && block_end >= data_end) {
    // copy a slice of the data block into the buffer
    memcpy(buffer, block_data + (data_start - block_start), size);
  }

  // requested data range starts in the block, but ends outside it
  if (block_start <= data_start && block_end < data_end) {
    // copy the data from data start until the block end to the buffer
    memcpy(buffer, block_data + (data_start - block_start), block_end - data_start + 1);
  }

  // request data range ends in the block, but starts outside it
  if (block_start > data_start && block_end >= data_end) {
    // copy the data from block start until the data end to the buffer
    memcpy(buffer + (block_start - data_start), block_data, data_end - block_start + 1);
  }

  // requested data range starts and ends outside the block
  if (block_start > data_start && block_end < data_end) {
    // copy the whole data block to the buffer
    memcpy(buffer + (block_start - data_start), block_data, MY_BLOCK_SIZE);
  }

  free(block_data);
}

void read_file_data(struct my_fcb* file_fcb, void* buffer, size_t size, off_t offset) {
  puts("Reading file data...");
  print_id(&(file_fcb->id));
  puts("\n");

  // read the index block from the database
  struct my_index index_block;
  read_db_object(file_fcb->data, &index_block, sizeof(index_block));

  // get the indexes of the first and last data block needed for reading
  int first_block, last_block;
  get_block_indexes(size, offset, &first_block, &last_block);

  // go through the data blocks and read data from them into the buffer
  for (int block = first_block; block <= last_block; block++) {
    read_block_to_buffer(index_block.entries[block], block, buffer, size, offset);
  }
}

void write_buffer_to_block(uuid_t id, int block_num, void* buffer, size_t size, off_t offset) {
  // read the data from the data block into memory
  // we need to do this because we will be writing the whole block back into
  // database, even though only a part of it may change
  void* block_data = malloc(MY_BLOCK_SIZE);
  read_db_object(id, block_data, MY_BLOCK_SIZE);

  /** @var Offset in the file of the first byte of the block */
  off_t block_start = block_num * MY_BLOCK_SIZE;
  /** @var Offset in the file of the last byte of the block */
  off_t block_end = block_start + MY_BLOCK_SIZE - 1;

  /** @var Offset in the file of the first byte of the read data */
  off_t data_start = offset;
  /** @var Offset in the file of the last byte of the read data */
  off_t data_end = offset + size - 1;

  // needed data range is fully contained in the block
  if (block_start <= data_start && block_end >= data_end) {
    // copy data from the whole buffer into the data block
    memcpy(block_data + (data_start - block_start), buffer, size);
  }

  // needed data range starts in the block, but ends outside it
  if (block_start <= data_start && block_end < data_end) {
    // copy data from the start of the buffer into the data block
    memcpy(block_data + (data_start - block_start), buffer, block_end - data_start + 1);
  }

  // needed data range ends in the block, but starts outside it
  if (block_start > data_start && block_end >= data_end) {
    // copy data from the end of the buffer into the data block
    memcpy(block_data, buffer + (block_start - data_start), data_end - block_start + 1);
  }

  // needed data range starts and ends outside the block
  if (block_start > data_start && block_end < data_end) {
    // copy data from the block into the whole data block
    memcpy(block_data, buffer + (block_start - data_start), MY_BLOCK_SIZE);
  }

  // save the changed data in the block back to the database
  write_db_object(id, block_data, MY_BLOCK_SIZE);
  free(block_data);
}

void write_file_data(struct my_fcb* file_fcb, void* buffer, size_t size, off_t offset) {
  puts("Writing file data...");
  print_id(&(file_fcb->id));
  puts("\n");

  // if we are writing outside the current file data, it needs to be expanded
  // first to be able to accommodate the new data
  if ((offset + size) > file_fcb->size) {
    truncate_file(file_fcb, offset + size);
  }

  // read the index block from the database
  struct my_index index_block;
  read_db_object(file_fcb->data, &index_block, sizeof(index_block));

  // get the indexes of the first and last data block needed for writing
  int first_block, last_block;
  get_block_indexes(size, offset, &first_block, &last_block);

  // go through the data blocks and write data to them from the buffer
  for (int block = first_block; block <= last_block; block++) {
    write_buffer_to_block(index_block.entries[block], block, buffer, size, offset);
  }
}

struct my_dir_entry* get_dir_entry(void* dir_data, int offset) {
  return dir_data + sizeof(struct my_dir_header) + offset * sizeof(struct my_dir_entry);
}

void add_dir_entry(struct my_fcb* dir_fcb, struct my_fcb* file_fcb, const char* name) {
  size_t data_size = dir_fcb->size;
  size_t max_size = data_size + sizeof(struct my_dir_entry);

  void* dir_data = malloc(max_size);
  read_file_data(dir_fcb, dir_data, data_size, 0);

  struct my_dir_header* dir_header = dir_data;
  struct my_dir_entry* free_entry;

  if (dir_header->first_free > -1) {
    free_entry = get_dir_entry(dir_data, dir_header->first_free);

    dir_header->first_free = free_entry->next_free;
  } else {
    free_entry = get_dir_entry(dir_data, dir_header->items);

    dir_header->items++;
    data_size = max_size;
  }

  strncpy(free_entry->name, name, MY_MAX_PATH - 1);
  uuid_copy(free_entry->fcb_id, file_fcb->id);
  free_entry->used = 1;

  write_file_data(dir_fcb, dir_data, data_size, 0);
  free(dir_data);
}

int remove_dir_entry(struct my_fcb* dir_fcb, const char* name) {
  void* dir_data = malloc(dir_fcb->size);
  read_file_data(dir_fcb, dir_data, dir_fcb->size, 0);

  struct my_dir_header* dir_header = dir_data;

  struct my_dir_entry* dir_entry;
  char found = 0;
  int offset = 0;

  for (; offset < dir_header->items; offset++) {
    dir_entry = get_dir_entry(dir_data, offset);

    if (!dir_entry->used) continue;

    if (strcmp(dir_entry->name, name) == 0) {
      found = 1;
      break;
    }
  }

  if (found) {
    memset(dir_entry, 0, sizeof(struct my_dir_entry));

    dir_entry->next_free = dir_header->first_free;
    dir_header->first_free = offset;

    write_file_data(dir_fcb, dir_data, dir_fcb->size, 0);

    return 0;
  } else {
    return -1;
  }
}

void iterate_dir_entries(struct my_fcb* dir_fcb, struct my_dir_iter* iter) {
  iter->position = 0;
  iter->dir_data = malloc(dir_fcb->size);

  read_file_data(dir_fcb, iter->dir_data, dir_fcb->size, 0);
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
  struct my_dir_iter iter;
  iterate_dir_entries(dir_fcb, &iter);

  int dir_size = 0;
  struct my_dir_entry* entry;

  while ((entry = next_dir_entry(&iter)) != NULL) {
    if (entry->used) dir_size++;
  }

  clean_dir_iterator(&iter);

  return dir_size;
}

void link_file(struct my_fcb* dir_fcb, struct my_fcb* file_fcb, const char* name) {
  add_dir_entry(dir_fcb, file_fcb, name);

  file_fcb->nlink++;
  update_file(file_fcb);
}

void unlink_file(struct my_fcb* dir_fcb, struct my_fcb* file_fcb, const char* name) {
  remove_dir_entry(dir_fcb, name);

  if (file_fcb->nlink > 0) {
    file_fcb->nlink--;
    update_file(file_fcb);
  }

  if (file_fcb->nlink == 0 && !is_file_open(file_fcb)) {
    remove_file(file_fcb);
  }
}

int find_file(const char* path, struct my_user user, struct my_fcb* file_fcb) {
  struct my_fcb dir_fcb;
  return find_dir_entry(path, user, &dir_fcb, file_fcb);
}

int find_dir_entry(const char* const_path, struct my_user user, struct my_fcb* dir_fcb, struct my_fcb* file_fcb) {
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

    if (!can_execute(file_fcb, user)) {
      free(full_path);
      return MYFS_FIND_NO_ACCESS;
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

char is_file(struct my_fcb* fcb) {
  return (fcb->mode & S_IFREG) == S_IFREG;
}

static char has_permission(struct my_fcb* fcb, struct my_user user,
    mode_t user_mode, mode_t group_mode, mode_t other_mode) {
  if (fcb->uid == user.uid) {
    return (fcb->mode & user_mode) == user_mode;
  } else if (fcb->gid == user.gid) {
    return (fcb->mode & group_mode) == group_mode;
  } else {
    return (fcb->mode & other_mode) == other_mode;
  }
}

struct my_user get_context_user() {
  struct fuse_context* context = fuse_get_context();

  struct my_user user = {
    .uid = context->uid,
    .gid = context->gid,
  };

  return user;
}

char can_read(struct my_fcb* fcb, struct my_user user) {
  return has_permission(fcb, user, S_IRUSR, S_IRGRP, S_IROTH);
}

char can_write(struct my_fcb* fcb, struct my_user user) {
  return has_permission(fcb, user, S_IWUSR, S_IWGRP, S_IWOTH);
}

char can_execute(struct my_fcb* fcb, struct my_user user) {
  return has_permission(fcb, user, S_IXUSR, S_IXGRP, S_IXOTH);
}

char check_open_flags(struct my_fcb* fcb, struct my_user user, int flags) {
  if (flags & O_RDWR) {
    return can_read(fcb, user) && can_write(fcb, user);
  } else if (flags & O_WRONLY) {
    return can_write(fcb, user);
  } else {
    return can_read(fcb, user);
  }
}

static int get_free_file_handle() {
  for (int fh = 0; fh < MY_MAX_OPEN_FILES; fh++) {
    if (!open_files[fh].used) {
      return fh;
    }
  }

  return -1;
}

static int find_open_file_handle(struct my_fcb* file) {
  for (int fh = 0; fh < MY_MAX_OPEN_FILES; fh++) {
    if (open_files[fh].used && uuid_compare(open_files[fh].id, file->id) == 0) {
      return fh;
    }
  }

  return -1;
}

int get_open_file(int fh, struct my_fcb* fcb) {
  if (open_files[fh].used) {
    read_db_object(open_files[fh].id, fcb, sizeof(struct my_fcb));
    return 0;
  } else {
    return -1;
  }
}

int add_open_file(struct my_fcb* file) {
  int fh = get_free_file_handle();

  if (fh != -1) {
    uuid_copy(open_files[fh].id, file->id);
    open_files[fh].used = 1;

    return fh;
  } else {
    return -1;
  }
}

int remove_open_file(int fh) {
  struct my_fcb file;

  if (get_open_file(fh, &file) == 0) {
    open_files[fh].used = 0;

    if (file.nlink == 0 && !is_file_open(&file)) {
      remove_file(&file);
    }

    return 0;
  } else {
    return -1;
  }
}

char is_file_open(struct my_fcb* file) {
  return find_open_file_handle(file) != -1;
}
