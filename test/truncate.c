#include <assert.h>
#include "../myfs_lib.h"

int main() {
  int rc = unqlite_open(&pDb, "truncate.db", UNQLITE_OPEN_CREATE);
  if (rc != UNQLITE_OK) error_handler(rc);

  struct my_user user = {1, 1};

  struct my_fcb file_fcb;
  create_file(0, user, &file_fcb);
  assert(file_fcb.size == 0);

  truncate_file(&file_fcb, 1000);
  assert(file_fcb.size == 1000);

  struct my_fcb check_fcb;
  read_file(&(file_fcb.id), &check_fcb);
  assert(check_fcb.size == file_fcb.size);

  void* check_data = malloc(check_fcb.size);
  read_file_data(&check_fcb, check_data, check_fcb.size, 0);

  void* empty_blob = calloc(1, check_fcb.size);
  assert(memcmp(check_data, empty_blob, check_fcb.size) == 0);

  puts("Test passed");

  unqlite_close(pDb);
}
