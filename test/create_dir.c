#include <assert.h>
#include "../myfs_lib.h"

int main() {
  int rc = unqlite_open(&pDb, "create_dir.db", UNQLITE_OPEN_CREATE);
	if (rc != UNQLITE_OK) error_handler(rc);

  struct my_fcb dir_fcb;
  create_directory(S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH, &dir_fcb);

  assert(dir_fcb.uid == getuid());
  assert(dir_fcb.gid == getgid());

  struct my_fcb check_fcb;
  read_file(dir_fcb.id, &check_fcb);

  assert(uuid_compare(dir_fcb.id, check_fcb.id) == 0);
  assert(check_fcb.mtime == dir_fcb.mtime);
  assert(check_fcb.mode & S_IFDIR);

  unqlite_close(pDb);
}
