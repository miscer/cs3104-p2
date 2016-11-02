#include "myfs.h"

void create_file(mode_t, struct my_fcb*);
void create_directory(mode_t, struct my_fcb*);
void read_file(uuid_t*, struct my_fcb*);
int find_file(const char*, struct my_fcb*);
int find_dir_entry(const char*, struct my_fcb*, struct my_fcb*);
void update_file(struct my_fcb);
void read_file_data(struct my_fcb, void*);
void write_file_data(struct my_fcb*, void*, size_t);
void add_dir_entry(struct my_fcb, struct my_fcb, const char*);
void remove_dir_entry(struct my_fcb, struct my_fcb);
char* path_file_name(const char*);
