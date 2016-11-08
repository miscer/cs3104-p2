#include <assert.h>
#include "../myfs_lib.h"

int main() {
  int rc = unqlite_open(&pDb, "find.db", UNQLITE_OPEN_CREATE);
	if (rc != UNQLITE_OK) error_handler(rc);

  struct my_fcb root_dir;
  create_directory(0, &root_dir);

  uuid_copy(root_object.id, root_dir.id);

  struct my_fcb file1;
  create_file(0, &file1);

  struct my_fcb file2;
  create_file(0, &file2);

  struct my_fcb file3;
  create_file(0, &file3);

  struct my_fcb dir1;
  create_directory(0, &dir1);

  add_dir_entry(&root_dir, &file1, "file1");
  add_dir_entry(&root_dir, &file2, "file2");
  add_dir_entry(&root_dir, &dir1, "dir1");
  add_dir_entry(&dir1, &file3, "file3");

  struct my_fcb found_file;

  find_file("/file1", &found_file);
  assert(uuid_compare(found_file.id, file1.id) == 0);

  find_file("/dir1/file3", &found_file);
  assert(uuid_compare(found_file.id, file3.id) == 0);

  assert(find_file("/foo", &found_file) == MYFS_FIND_NO_FILE);
  assert(find_file("/foo/bar", &found_file) == MYFS_FIND_NO_DIR);
  assert(find_file("/dir1/foo", &found_file) == MYFS_FIND_NO_FILE);
  assert(find_file("/file1/foo", &found_file) == MYFS_FIND_NO_DIR);

  puts("Test passed");

  unqlite_close(pDb);
}
