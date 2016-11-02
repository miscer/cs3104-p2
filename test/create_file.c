#include <assert.h>
#include "../myfs_lib.h"

int main() {
  int rc = unqlite_open(&pDb, "create_file.db", UNQLITE_OPEN_CREATE);
	if (rc != UNQLITE_OK) error_handler(rc);

  struct my_fcb file_fcb;
  create_file(S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH, &file_fcb);

  assert(file_fcb.uid == getuid());
  assert(file_fcb.gid == getgid());

  struct my_fcb check_fcb;
  unqlite_int64 size = sizeof(struct my_fcb);
  unqlite_kv_fetch(pDb, file_fcb.id, KEY_SIZE, &check_fcb, &size);

  assert(uuid_compare(file_fcb.id, check_fcb.id) == 0);
  assert(check_fcb.mtime == file_fcb.mtime);

  unqlite_close(pDb);
}
