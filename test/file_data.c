#include <assert.h>
#include "../myfs_lib.h"

void test_whole() {
  struct my_user user = {1, 1};

  struct my_fcb file_fcb;
  create_file(0, user, &file_fcb);

  size_t size = MY_BLOCK_SIZE * 50;
  int* data_src = malloc(size);

  write_file_data(&file_fcb, data_src, size, 0);

  struct my_fcb check_fcb;
  read_file(&(file_fcb.id), &check_fcb);

  int* data_check = malloc(size);
  read_file_data(check_fcb, data_check, size, 0);

  assert(memcmp(data_src, data_check, size) == 0);
}

void test_part() {
  struct my_user user = {1, 1};

  struct my_fcb file_fcb;
  create_file(0, user, &file_fcb);

  off_t offset = MY_BLOCK_SIZE * 20;
  size_t size = MY_BLOCK_SIZE * 20;
  int* data_src = malloc(size);

  write_file_data(&file_fcb, data_src, size, offset);

  struct my_fcb check_fcb;
  read_file(&(file_fcb.id), &check_fcb);

  int* data_check = malloc(size);
  read_file_data(check_fcb, data_check, size, offset);

  assert(memcmp(data_src, data_check, size) == 0);
  assert(check_fcb.size == (offset + size));
}

int main() {
  int rc = unqlite_open(&pDb, "create_file.db", UNQLITE_OPEN_CREATE);
  if (rc != UNQLITE_OK) error_handler(rc);

  test_whole();
  test_part();

  puts("Test passed");

  unqlite_close(pDb);
}
