#include <assert.h>
#include "../myfs_lib.h"

int main() {
  int rc = unqlite_open(&pDb, "database_test.db", UNQLITE_OPEN_CREATE);
  if (rc != UNQLITE_OK) error_handler(rc);

  uuid_t key;
  uuid_generate(key);

  assert(!has_db_object(key));

  void* data_src = malloc(1000);
  write_db_object(key, data_src, 1000);

  assert(has_db_object(key));

  void* data_check = malloc(1000);
  read_db_object(key, data_check, 1000);

  assert(memcmp(data_src, data_check, 1000) == 0);

  delete_db_object(key);
  assert(!has_db_object(key));

  puts("Test passed");

  return 0;
}
