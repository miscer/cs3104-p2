#include <assert.h>
#include "../myfs_lib.h"

int main() {
  int rc = unqlite_open(&pDb, "create_file.db", UNQLITE_OPEN_CREATE);
  if (rc != UNQLITE_OK) error_handler(rc);

  struct my_user user = {1, 1};

  struct my_fcb file_fcb;
  create_file(S_IRUSR, user, &file_fcb);

  assert(!is_directory(&file_fcb));
  assert(file_fcb.mode & S_IRUSR);
  assert(file_fcb.uid == user.uid);
  assert(file_fcb.gid == user.gid);

  struct my_fcb check_fcb;
  read_file(&(file_fcb.id), &check_fcb);

  assert(uuid_compare(file_fcb.id, check_fcb.id) == 0);
  assert(check_fcb.mtime == file_fcb.mtime);

  puts("Test passed");

  unqlite_close(pDb);
}
