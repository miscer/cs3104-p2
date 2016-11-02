#include <assert.h>
#include "../myfs_lib.h"

int main() {
  int rc = unqlite_open(&pDb, "update_file.db", UNQLITE_OPEN_CREATE);
	if (rc != UNQLITE_OK) error_handler(rc);

  struct my_fcb file_fcb;
  create_file(S_IRUSR, &file_fcb);

  file_fcb.mode |= S_IWUSR;
  update_file(file_fcb);

  struct my_fcb check_fcb;
  unqlite_int64 size = sizeof(struct my_fcb);
  unqlite_kv_fetch(pDb, file_fcb.id, KEY_SIZE, &check_fcb, &size);

  assert((check_fcb.mode & (S_IRUSR|S_IWUSR)) == (S_IRUSR|S_IWUSR));

  unqlite_close(pDb);
}
