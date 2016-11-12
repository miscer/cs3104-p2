#include <assert.h>
#include "../myfs_lib.h"

int main() {
  int rc = unqlite_open(&pDb, "find.db", UNQLITE_OPEN_CREATE);
  if (rc != UNQLITE_OK) error_handler(rc);

  struct my_user user = {1, 1};

  struct my_fcb root_dir;
  create_directory(S_IXUSR, user, &root_dir);

  uuid_copy(root_object.id, root_dir.id);

  struct my_fcb file1;
  create_file(0, user, &file1);

  struct my_fcb file2;
  create_file(0, user, &file2);

  struct my_fcb file3;
  create_file(0, user, &file3);

  struct my_fcb dir1;
  create_directory(S_IXUSR, user, &dir1);

  add_dir_entry(&root_dir, &file1, "file1");
  add_dir_entry(&root_dir, &file2, "file2");
  add_dir_entry(&root_dir, &dir1, "dir1");
  add_dir_entry(&dir1, &file3, "file3");

  struct my_fcb found_file;

  find_file("/file1", user, &found_file);
  assert(uuid_compare(found_file.id, file1.id) == 0);

  find_file("/dir1/file3", user, &found_file);
  assert(uuid_compare(found_file.id, file3.id) == 0);

  assert(find_file("/foo", user, &found_file) == MYFS_FIND_NO_FILE);
  assert(find_file("/foo/bar", user, &found_file) == MYFS_FIND_NO_DIR);
  assert(find_file("/dir1/foo", user, &found_file) == MYFS_FIND_NO_FILE);
  assert(find_file("/file1/foo", user, &found_file) == MYFS_FIND_NO_DIR);

  dir1.mode &= ~S_IXUSR;
  update_file(&dir1);

  assert(find_file("/dir1/file3", user, &found_file) == MYFS_FIND_NO_ACCESS);
  assert(find_file("/dir1/foo", user, &found_file) == MYFS_FIND_NO_ACCESS);

  puts("Test passed");

  unqlite_close(pDb);
}
