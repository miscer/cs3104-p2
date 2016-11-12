#include <assert.h>
#include "../myfs_lib.h"

int main() {
  int rc = unqlite_open(&pDb, "open.db", UNQLITE_OPEN_CREATE);
  if (rc != UNQLITE_OK) error_handler(rc);

  struct my_user user = {1, 1};

  struct my_fcb file1;
  create_file(S_IRUSR, user, &file1);

  struct my_fcb file2;
  create_file(S_IRUSR, user, &file2);

  struct my_fcb check_fcb;

  assert(!is_file_open(&file1));
  assert(!is_file_open(&file2));

  int fh1 = add_open_file(&file1);
  int fh2 = add_open_file(&file1);
  assert(fh1 > -1);
  assert(fh2 > -1);
  assert(fh1 != fh2);

  assert(is_file_open(&file1));
  assert(!is_file_open(&file2));

  int fh3 = add_open_file(&file2);
  assert(fh3 > -1);

  assert(is_file_open(&file2));

  assert(get_open_file(fh1, &check_fcb) == 0);
  assert(uuid_compare(check_fcb.id, file1.id) == 0);

  assert(get_open_file(fh2, &check_fcb) == 0);
  assert(uuid_compare(check_fcb.id, file1.id) == 0);

  assert(remove_open_file(fh1) == 0);
  assert(get_open_file(fh1, &check_fcb) < 0);
  assert(get_open_file(fh2, &check_fcb) == 0);
  assert(is_file_open(&file1));

  assert(remove_open_file(fh2) == 0);
  assert(get_open_file(fh2, &check_fcb) < 0);
  assert(!is_file_open(&file1));

  assert(read_file(&(file1.id), &check_fcb) < 0);

  puts("Test passed");

  return 0;
}
