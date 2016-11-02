#include <assert.h>
#include "../myfs_lib.h"

int main() {
  int rc = unqlite_open(&pDb, "create_file.db", UNQLITE_OPEN_CREATE);
	if (rc != UNQLITE_OK) error_handler(rc);

  struct my_fcb file_fcb;
  create_file(0, &file_fcb);

  int* data_src = malloc(sizeof(int));
  *data_src = 12345678;

  write_file_data(&file_fcb, data_src, sizeof(int));

  struct my_fcb check_fcb;
  read_file(&(file_fcb.id), &check_fcb);

  int* data_check = malloc(check_fcb.size);
  read_file_data(check_fcb, data_check);

  assert(*data_check == 12345678);

  puts("Test passed");

  unqlite_close(pDb);
}
