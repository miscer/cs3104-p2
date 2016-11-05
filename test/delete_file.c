#include <assert.h>
#include "../myfs_lib.h"

int main() {
  int rc = unqlite_open(&pDb, "delete_file.db", UNQLITE_OPEN_CREATE);
	if (rc != UNQLITE_OK) error_handler(rc);

  struct my_fcb file_fcb;
  create_file(S_IRUSR, &file_fcb);

  uuid_t file_id;
  uuid_copy(file_id, file_fcb.id);

  remove_file(&file_fcb);

  struct my_fcb check_fcb;
  rc = read_file(&file_id, &check_fcb);
  assert(rc < 0);

  puts("Test passed");

  unqlite_close(pDb);
}
