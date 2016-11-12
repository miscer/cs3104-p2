#include <assert.h>
#include "../myfs_lib.h"

struct my_user user = {1, 1};

void test_read_write(size_t size, off_t offset) {
  printf("Testing R&W for size %zu and offset %zu\n", size, offset);

  struct my_fcb file_fcb;
  create_file(0, user, &file_fcb);
  printf("Created file\n");

  void* data_src = malloc(size);
  assert(data_src != NULL);
  printf("Allocated source\n");

  write_file_data(&file_fcb, data_src, size, offset);
  printf("Wrote source data\n");

  struct my_fcb check_fcb;
  read_file(&(file_fcb.id), &check_fcb);
  printf("Read file\n");

  int* data_check = malloc(size);
  assert(data_check != NULL);
  printf("Allocated check\n");

  read_file_data(&check_fcb, data_check, size, offset);
  printf("Read check data\n");

  assert(memcmp(data_src, data_check, size) == 0);
  printf("All good\n");

  free(data_src);
  free(data_check);
}

int main() {
  int rc = unqlite_open(&pDb, "file_data.db", UNQLITE_OPEN_CREATE);
  if (rc != UNQLITE_OK) error_handler(rc);

  // single block
  test_read_write(MY_BLOCK_SIZE, 0);
  test_read_write(MY_BLOCK_SIZE, MY_BLOCK_SIZE);
  test_read_write(MY_BLOCK_SIZE / 2, MY_BLOCK_SIZE / 4);

  // two blocks
  test_read_write(2 * MY_BLOCK_SIZE, 0);
  test_read_write(MY_BLOCK_SIZE, MY_BLOCK_SIZE / 2);

  // multiple blocks
  test_read_write(5 * MY_BLOCK_SIZE, 0);
  test_read_write(5 * MY_BLOCK_SIZE, MY_BLOCK_SIZE / 2);

  // all blocks
  test_read_write(MY_MAX_BLOCKS * MY_BLOCK_SIZE, 0);

  puts("Test passed");

  unqlite_close(pDb);
}
