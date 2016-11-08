#include <assert.h>
#include "../myfs_lib.h"

int main() {
  int rc = unqlite_open(&pDb, "unlink_file.db", UNQLITE_OPEN_CREATE);
	if (rc != UNQLITE_OK) error_handler(rc);

  struct my_fcb dir_fcb;
  create_directory(0, &dir_fcb);

  uuid_copy(root_object.id, dir_fcb.id);

  struct my_fcb file_fcb;
  create_file(0, &file_fcb);

  uuid_t file_id;
  uuid_copy(file_id, file_fcb.id);

  link_file(&dir_fcb, &file_fcb, "file1");
  link_file(&dir_fcb, &file_fcb, "file2");
  link_file(&dir_fcb, &file_fcb, "file3");

  unlink_file(&dir_fcb, &file_fcb, "file2");
  unlink_file(&dir_fcb, &file_fcb, "file3");

  struct my_fcb check_fcb;
  assert(find_file("/file1", &check_fcb) == MYFS_FIND_FOUND);
  assert(find_file("/file2", &check_fcb) == MYFS_FIND_NO_FILE);
  assert(find_file("/file3", &check_fcb) == MYFS_FIND_NO_FILE);

  rc = read_file(&file_id, &check_fcb);
  assert(rc == 0);

  unlink_file(&dir_fcb, &file_fcb, "file1");
  assert(find_file("/file1", &check_fcb) == MYFS_FIND_NO_FILE);

  rc = read_file(&file_id, &check_fcb);
  assert(rc < 0);

  puts("Test passed");

  unqlite_close(pDb);
}
