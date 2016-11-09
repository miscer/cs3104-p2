#include <assert.h>
#include "../myfs_lib.h"

int main() {
  int rc = unqlite_open(&pDb, "update_file.db", UNQLITE_OPEN_CREATE);
  if (rc != UNQLITE_OK) error_handler(rc);

  struct my_user user = {1, 1};

  struct my_fcb file_fcb;
  create_file(S_IRUSR, user, &file_fcb);

  file_fcb.mode |= S_IWUSR;
  update_file(file_fcb);

  struct my_fcb check_fcb;
  read_file(&(file_fcb.id), &check_fcb);

  assert((check_fcb.mode & (S_IRUSR|S_IWUSR)) == (S_IRUSR|S_IWUSR));

  puts("Test passed");

  unqlite_close(pDb);
}
