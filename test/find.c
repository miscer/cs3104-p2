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

  add_dir_entry(&root_dir, &file1, "file1");
  add_dir_entry(&root_dir, &file2, "file2");

  struct my_fcb found_file;
  find_file("/file1", &found_file);

  assert(uuid_compare(found_file.id, file1.id) == 0);

  puts("Test passed");

  unqlite_close(pDb);
}
