#include <assert.h>
#include "../myfs_lib.h"

int main() {
  int rc = unqlite_open(&pDb, "dir_entry.db", UNQLITE_OPEN_CREATE);
  if (rc != UNQLITE_OK) error_handler(rc);

  struct my_user user = {1, 1};

  struct my_fcb file1;
  create_file(0, user, &file1);

  struct my_fcb file2;
  create_file(0, user, &file2);

  struct my_fcb file3;
  create_file(0, user, &file3);

  struct my_fcb file4;
  create_file(0, user, &file4);

  struct my_fcb dir;
  create_directory(0, user, &dir);

  add_dir_entry(&dir, &file1, "file1");
  add_dir_entry(&dir, &file2, "file2");
  add_dir_entry(&dir, &file3, "file3");

  assert(get_directory_size(&dir) == 3);

  remove_dir_entry(&dir, "file1");
  remove_dir_entry(&dir, "file2");

  assert(get_directory_size(&dir) == 1);
  
  add_dir_entry(&dir, &file2, "file4");
  add_dir_entry(&dir, &file4, "file5");

  assert(get_directory_size(&dir) == 3);

  struct my_dir_iter iter;
  iterate_dir_entries(&dir, &iter);

  struct my_dir_entry* entry;

  entry = next_dir_entry(&iter);
  assert(entry != NULL);
  assert(strcmp(entry->name, "file5") == 0);
  assert(uuid_compare(entry->fcb_id, file4.id) == 0);

  entry = next_dir_entry(&iter);
  assert(entry != NULL);
  assert(strcmp(entry->name, "file4") == 0);
  assert(uuid_compare(entry->fcb_id, file2.id) == 0);

  entry = next_dir_entry(&iter);
  assert(entry != NULL);
  assert(strcmp(entry->name, "file3") == 0);
  assert(uuid_compare(entry->fcb_id, file3.id) == 0);

  entry = next_dir_entry(&iter);
  assert(entry == NULL);

  clean_dir_iterator(&iter);

  remove_dir_entry(&dir, "file5");
  assert(get_directory_size(&dir) == 2);

  puts("Test passed");

  unqlite_close(pDb);
}
