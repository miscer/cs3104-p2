#include "myfs.h"

#define MYFS_FIND_FOUND 0
#define MYFS_FIND_NO_DIR -1
#define MYFS_FIND_NO_FILE -2

struct my_dir_iter {
  int position;
  void* dir_data;
};

void create_file(mode_t, struct my_fcb*);
void create_directory(mode_t, struct my_fcb*);
int read_file(uuid_t*, struct my_fcb*);
int find_file(const char*, struct my_fcb*);
int find_dir_entry(const char*, struct my_fcb*, struct my_fcb*);
void update_file(struct my_fcb);
void remove_file(struct my_fcb*);
void read_file_data(struct my_fcb, void*);
void write_file_data(struct my_fcb*, void*, size_t);
void add_dir_entry(struct my_fcb*, struct my_fcb*, const char*);
int remove_dir_entry(struct my_fcb*, struct my_fcb*);
void iterate_dir_entries(struct my_fcb*, struct my_dir_iter*);
struct my_dir_entry* next_dir_entry(struct my_dir_iter*);
void clean_dir_iterator(struct my_dir_iter*);
char* path_split(char**);
char* path_file_name(char*);
char is_directory(struct my_fcb*);
