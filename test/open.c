#include <assert.h>
#include "../myfs_lib.h"

int main() {
  int rc = unqlite_open(&pDb, "open.db", UNQLITE_OPEN_CREATE);
  if (rc != UNQLITE_OK) error_handler(rc);

  struct my_user user = {1, 1};

  struct my_fcb file_fcb;
  create_file(S_IRUSR, user, &file_fcb);

  struct my_fcb check_fcb;
  assert(get_open_file(123, &check_fcb) < 0);

  int fh = add_open_file(&file_fcb);
  assert(fh > -1);

  assert(get_open_file(fh, &check_fcb) == 0);
  assert(uuid_compare(check_fcb.id, file_fcb.id) == 0);

  assert(remove_open_file(fh) == 0);
  assert(get_open_file(fh, &check_fcb) < 0);

  puts("Test passed");

  return 0;
}
