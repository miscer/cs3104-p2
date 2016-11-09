#include <assert.h>
#include "../myfs_lib.h"

int main() {
  int rc = unqlite_open(&pDb, "create_dir.db", UNQLITE_OPEN_CREATE);
  if (rc != UNQLITE_OK) error_handler(rc);

  struct my_user user = {1, 1};

  struct my_fcb dir_fcb;
  create_directory(S_IRUSR, user, &dir_fcb);

  assert(is_directory(&dir_fcb));
  assert(dir_fcb.mode & S_IRUSR);
  assert(dir_fcb.uid == user.uid);
  assert(dir_fcb.gid == user.gid);

  struct my_fcb check_fcb;
  read_file(&(dir_fcb.id), &check_fcb);

  assert(uuid_compare(dir_fcb.id, check_fcb.id) == 0);
  assert(check_fcb.mtime == dir_fcb.mtime);
  assert(is_directory(&check_fcb));

  puts("Test passed");

  unqlite_close(pDb);
}
