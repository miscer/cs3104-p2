/*
  MyFS. One directory, one file, 1000 bytes of storage. What more do you need?

  This Fuse file system is based largely on the HelloWorld example by Miklos Szeredi <miklos@szeredi.hu> (http://fuse.sourceforge.net/helloworld.html). Additional inspiration was taken from Joseph J. Pfeiffer's "Writing a FUSE Filesystem: a Tutorial" (http://www.cs.nmsu.edu/~pfeiffer/fuse-tutorial/).
*/

#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <errno.h>
#include <fcntl.h>

#include "myfs.h"

/**
 * @var Open file table
 * Array indexes are the file handles used to identify open files
 */
static struct my_open_file open_files[MY_MAX_OPEN_FILES];

// Get file and directory attributes (meta-data).
// Read 'man 2 stat' and 'man 2 chmod'.
static int myfs_getattr(const char *path, struct stat *stbuf){
  write_log("myfs_getattr(path=\"%s\", statbuf=0x%08x)\n", path, stbuf);

  struct my_fcb file_fcb;

  // try to find the file by its path and current user
  int result = find_file(path, get_context_user(), &file_fcb);

  if (result == MYFS_FIND_NO_ACCESS) {
    // user cannot access one of the parent directories
    write_log("myfs_getattr - EACCES\n");
    return -EACCES;

  } else if (result != MYFS_FIND_FOUND) {
    // file does not exist
    write_log("myfs_getattr - ENOENT\n");
    return -ENOENT;
  }

  // clear the stat struct
  memset(stbuf, 0, sizeof(struct stat));

  // copy values from the FCB into the stat struct
  stbuf->st_mode = file_fcb.mode;
  stbuf->st_nlink = file_fcb.nlink;
  stbuf->st_uid = file_fcb.uid;
  stbuf->st_gid = file_fcb.gid;
  stbuf->st_size = file_fcb.size;
  stbuf->st_atime = file_fcb.atime;
  stbuf->st_mtime = file_fcb.mtime;
  stbuf->st_ctime = file_fcb.ctime;

  return 0;
}

// Read a directory.
// Read 'man 2 readdir'.
static int myfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi){
  write_log("write_readdir(path=\"%s\", buf=0x%08x, filler=0x%08x, offset=%lld, fi=0x%08x)\n", path, buf, filler, offset, fi);

  // get the FCB of the directory by the file handle returned by opendir
  struct my_fcb dir_fcb;
  get_open_file(fi->fh, &dir_fcb);

  filler(buf, ".", NULL, 0);
  filler(buf, "..", NULL, 0);

  // iterate directory entries
  struct my_dir_iter iter;
  iterate_dir_entries(&dir_fcb, &iter);

  struct my_dir_entry* entry;

  while ((entry = next_dir_entry(&iter)) != NULL) {
    filler(buf, entry->name, NULL, 0);
  }

  clean_dir_iterator(&iter);

  return 0;
}

// Read a file.
// Read 'man 2 read'.
static int myfs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi){
  write_log("myfs_read(path=\"%s\", buf=0x%08x, size=%d, offset=%lld, fi=0x%08x)\n", path, buf, size, offset, fi);

  // get the FCB of the file by the file handle returned by open
  struct my_fcb file_fcb;
  get_open_file(fi->fh, &file_fcb);

  if (file_fcb.size == 0) {
    // file is empty, nothing to see here
    return 0;

  } else if (offset >= file_fcb.size) {
    // cannot read beyond the end of the file
    return 0;

  } else if ((offset + size) > file_fcb.size) {
    // cannot read beyond the end of file, but can read until it
    size = file_fcb.size - offset;

    read_file_data(&file_fcb, buf, size, offset);
    return size;

  } else {
    // read range is ok
    read_file_data(&file_fcb, buf, size, offset);
    return size;
  }
}

// Create a file.
// Read 'man 2 creat'.
static int myfs_create(const char *path, mode_t mode, struct fuse_file_info *fi){
  write_log("myfs_create(path=\"%s\", mode=0%03o, fi=0x%08x)\n", path, mode, fi);

  /** @var Parent directory FCB */
  struct my_fcb dir_fcb;
  /** @var Created file FCB */
  struct my_fcb file_fcb;

  // try to find the file (to check if it exists) and parent directory (to add to)
  int result = find_dir_entry(path, get_context_user(), &dir_fcb, &file_fcb);

  if (result == MYFS_FIND_NO_DIR) {
    // parent directory does not exist
    write_log("myfs_create - ENOENT\n");
    return -ENOENT;

  } else if (result == MYFS_FIND_FOUND) {
    // file already exists
    write_log("myfs_create - EEXIST\n");
    return -EEXIST;

  } else if (
    result == MYFS_FIND_NO_ACCESS ||
    !can_write(&dir_fcb, get_context_user())
  ) {
    // user cannot access ancestor directory, or cannot write to parent directory
    write_log("myfs_create - EACCES\n");
    return -EACCES;

  } else {
    // file does not exist and user has sufficient rights

    // create a new file and write it to the database, save FCB to file_fcb
    create_file(mode, get_context_user(), &file_fcb);

    // get the file name from the path and add the file to the parent directory
    // path string needs to be duplicated because it will be modified by path_file_name
    char* path_dup = strdup(path);
    char* file_name = path_file_name(path_dup);
    int result = link_file(&dir_fcb, &file_fcb, file_name);
    free(path_dup);

    if (result < 0) {
      // the parent directory does not have space left to add the file
      remove_file(&file_fcb);
      write_log("myfs_create - EFBIG\n");
      return -EFBIG;
    }

    // add the file to open file table and get its file handle
    int fh = add_open_file(&file_fcb);

    if (fh < 0) {
      // too many files are open
      write_log("myfs_create - ENFILE\n");
      return -ENFILE;
    }

    // save the file handle for future calls
    fi->fh = fh;

    return 0;
  }
}

// Set update the times (actime, modtime) for a file.
// Read 'man 2 utime'.
static int myfs_utime(const char *path, struct utimbuf *ubuf){
  write_log("myfs_utime(path=\"%s\", ubuf=0x%08x)\n", path, ubuf);

  struct my_fcb file_fcb;

  // try to find the file by its path and current user
  int result = find_file(path, get_context_user(), &file_fcb);

  if (
    result == MYFS_FIND_NO_DIR ||
    result == MYFS_FIND_NO_FILE
  ) {
    // file does not exist
    write_log("myfs_utime - ENOENT\n");
    return -ENOENT;

  } else if (
    result == MYFS_FIND_NO_ACCESS ||
    !can_write(&file_fcb, get_context_user())
  ) {
    // user cannot access an ancestor directory or write to the file
    write_log("myfs_utime - EACCES\n");
    return -EACCES;
  }

  // update the FCB
  file_fcb.atime = ubuf->actime;
  file_fcb.mtime = ubuf->modtime;

  // write the updated FCB into the database
  update_file(&file_fcb);

  return 0;
}

// Write to a file.
// Read 'man 2 write'
static int myfs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi){
  write_log("myfs_write(path=\"%s\", buf=0x%08x, size=%d, offset=%lld, fi=0x%08x)\n", path, buf, size, offset, fi);

  // get the FCB by the file handle
  struct my_fcb file_fcb;
  get_open_file(fi->fh, &file_fcb);

  if (offset >= MY_MAX_FILE_SIZE) {
    // cannot writer beyond the maximum file size
    write_log("myfs_write - EFBIG\n");
    return -EFBIG;

  } else if ((offset + size) > MY_MAX_FILE_SIZE) {
    // cannot write beyond the maximum file size, but can write until it
    size = MY_MAX_FILE_SIZE - offset;

    write_file_data(&file_fcb, (char*)buf, size, offset);
    return size;

  } else {
    // write range is ok
    write_file_data(&file_fcb, (char*)buf, size, offset);
    return size;
  }
}

// Set the size of a file.
// Read 'man 2 truncate'.
static int myfs_truncate(const char *path, off_t newsize){
  write_log("myfs_truncate(path=\"%s\", newsize=%lld)\n", path, newsize);

  struct my_fcb file_fcb;

  // try to find the file by its path
  int result = find_file(path, get_context_user(), &file_fcb);

  if (
    result == MYFS_FIND_NO_DIR ||
    result == MYFS_FIND_NO_FILE ||
    !is_file(&file_fcb)
  ) {
    // file does not exist, or is not a regular file
    write_log("myfs_truncate - ENOENT\n");
    return -ENOENT;
  }

  else if (
    result == MYFS_FIND_NO_ACCESS ||
    !can_write(&file_fcb, get_context_user())
  ) {
    // cannot access an ancestor directory, or cannot write to the file
    write_log("myfs_getattr - EACCES\n");
    return -EACCES;
  }

  // change the file size
  truncate_file(&file_fcb, newsize);

  return 0;
}

// Set permissions.
// Read 'man 2 chmod'.
static int myfs_chmod(const char *path, mode_t mode){
  write_log("myfs_chmod(fpath=\"%s\", mode=0%03o)\n", path, mode);

  struct my_user user = get_context_user();

  // try to find the file by its path
  struct my_fcb file_fcb;
  int result = find_file(path, user, &file_fcb);

  if (
    result == MYFS_FIND_NO_DIR ||
    result == MYFS_FIND_NO_FILE
  ) {
    // file does not exist
    write_log("myfs_chmod - ENOENT\n");
    return -ENOENT;

  } else if (result == MYFS_FIND_NO_ACCESS) {
    // file cannot be accessed
    write_log("myfs_getattr - EACCES\n");
    return -EACCES;

  } else if (file_fcb.uid != user.uid) {
    // the current user is not the owner of the file
    write_log("myfs_getattr - EPERM\n");
    return -EPERM;
  }

  // update the FCB and write it to the database
  file_fcb.mode = mode;
  file_fcb.ctime = time(0);
  update_file(&file_fcb);

  return 0;
}

// Set ownership.
// Read 'man 2 chown'.
static int myfs_chown(const char *path, uid_t uid, gid_t gid){
  write_log("myfs_chown(path=\"%s\", uid=%d, gid=%d)\n", path, uid, gid);

  struct my_user user = get_context_user();

  // try to find the file by its path
  struct my_fcb file_fcb;
  int result = find_file(path, user, &file_fcb);

  if (
    result == MYFS_FIND_NO_DIR ||
    result == MYFS_FIND_NO_FILE
  ) {
    // file does not exist
    write_log("myfs_chmod - ENOENT\n");
    return -ENOENT;

  } else if (result == MYFS_FIND_NO_ACCESS) {
    // file cannot be accessed
    write_log("myfs_getattr - EACCES\n");
    return -EACCES;
  }

  // update the FCB and write it to the database
  file_fcb.uid = uid;
  file_fcb.gid = gid;
  file_fcb.ctime = time(0);
  update_file(&file_fcb);

  return 0;
}

// Create a directory.
// Read 'man 2 mkdir'.
static int myfs_mkdir(const char *path, mode_t mode){
  write_log("myfs_mkdir: %s\n", path);

  struct my_fcb parent_fcb;
  struct my_fcb dir_fcb;

  // try to find the directory and its parent directory by its path
  int result = find_dir_entry(path, get_context_user(), &parent_fcb, &dir_fcb);

  if (result == MYFS_FIND_NO_DIR) {
    // parent directory does not exist
    write_log("myfs_mkdir - ENOENT\n");
    return -ENOENT;

  } else if (result == MYFS_FIND_FOUND) {
    // directory already exists
    write_log("myfs_mkdir - EEXIST\n");
    return -EEXIST;

  } else if (result == MYFS_FIND_NO_ACCESS) {
    // user cannot access the parent directory
    write_log("myfs_mkdir - EACCES\n");
    return -EACCES;

  } else {
    // create a new directory and write it to the database
    create_directory(mode, get_context_user(), &dir_fcb);

    // get the directory name from the path and add the directory to the parent directory
    // path string needs to be duplicated because it will be modified by path_file_name
    char* path_dup = strdup(path);
    char* dir_name = path_file_name(path_dup);
    int result = link_file(&parent_fcb, &dir_fcb, dir_name);
    free(path_dup);

    if (result < 0) {
      // the parent directory does not have space left to add the directory
      remove_file(&dir_fcb);
      write_log("myfs_mkdir - EFBIG\n");
      return -EFBIG;
    }

    return 0;
  }
}

// Delete a file.
// Read 'man 2 unlink'.
static int myfs_unlink(const char *path){
  write_log("myfs_unlink: %s\n",path);

  struct my_fcb dir_fcb;
  struct my_fcb file_fcb;

  // try to find the file and its parent directory by its path
  int result = find_dir_entry(path, get_context_user(), &dir_fcb, &file_fcb);

  if (
    result == MYFS_FIND_NO_DIR ||
    result == MYFS_FIND_NO_FILE
  ) {
    // file does not exist
    write_log("myfs_unlink - ENOENT\n");
    return -ENOENT;

  } else if (
    result == MYFS_FIND_NO_ACCESS ||
    !can_write(&dir_fcb, get_context_user())
  ) {
    // user cannot access the file or write to the parent directory
    write_log("myfs_unlink - EACCES\n");
    return -EACCES;

  } else if (!is_file(&file_fcb)) {
    // the file is not a regular file
    write_log("myfs_unlink - EPERM\n");
    return -EPERM;
  }

  // get the file name from the path and remove the file from the parent directory
  // path string needs to be duplicated because it will be modified by path_file_name
  // if no other links point to the file and it is not open, it will be deleted
  char* path_dup = strdup(path);
  char* file_name = path_file_name(path_dup);
  unlink_file(&dir_fcb, &file_fcb, file_name);
  free(path_dup);

  return 0;
}

// Delete a directory.
// Read 'man 2 rmdir'.
static int myfs_rmdir(const char *path){
  write_log("myfs_rmdir: %s\n",path);

  struct my_fcb parent_fcb;
  struct my_fcb dir_fcb;

  // try to find the drectory and its parent directory by its path
  int result = find_dir_entry(path, get_context_user(), &parent_fcb, &dir_fcb);

  if (
    result == MYFS_FIND_NO_DIR ||
    result == MYFS_FIND_NO_FILE
  ) {
    // directory does not exist
    write_log("myfs_rmdir - ENOENT\n");
    return -ENOENT;

  } else if (
    result == MYFS_FIND_NO_ACCESS ||
    !can_write(&parent_fcb, get_context_user())
  ) {
    // user cannot access the directory or write to the parent directory
    write_log("myfs_rmdir - EACCES\n");
    return -EACCES;

  } else if (!is_directory(&dir_fcb)) {
    // the found FCB is not a directory
    write_log("myfs_rmdir - ENOTDIR\n");
    return -ENOTDIR;

  } else if (get_directory_size(&dir_fcb) != 0) {
    // the directory is not empty
    write_log("myfs_rmdir - ENOTEMPTY\n");
    return -ENOTEMPTY;
  }

  // get the directory name from the path and remove the directory from the parent directory
  // path string needs to be duplicated because it will be modified by path_file_name
  // if the directory is not open, it will be deleted
  char* path_dup = strdup(path);
  char* file_name = path_file_name(path_dup);
  unlink_file(&parent_fcb, &dir_fcb, file_name);
  free(path_dup);

  return 0;
}

// Create a hard link to the file.
// Read 'man 2 link'.
static int myfs_link(const char* from, const char* to) {
  write_log("myfs_link(from=\"%s\", to=\"%s\")\n", from, to);
  int result;

  // try to find the linked file
  struct my_fcb from_fcb;
  result = find_file(from, get_context_user(), &from_fcb);

  if (result == MYFS_FIND_NO_ACCESS) {
    // user cannot access the linked file's parent directory
    write_log("myfs_link - EACCES\n");
    return -EACCES;

  } else if (result != MYFS_FIND_FOUND) {
    // linked file does not exist
    write_log("myfs_link - ENOENT\n");
    return -ENOENT;

  } else if (is_directory(&from_fcb)) {
    // linked file is a directory
    write_log("myfs_link - EPERM\n");
    return -EPERM;
  }

  struct my_fcb to_fcb;
  struct my_fcb dir_fcb;

  // try to find the file that would be replaced by the link
  result = find_dir_entry(to, get_context_user(), &dir_fcb, &to_fcb);

  if (result == MYFS_FIND_NO_DIR) {
    // parent directory for the link does not exist
    write_log("myfs_link - ENOENT\n");
    return -ENOENT;

  } else if (result == MYFS_FIND_NO_ACCESS) {
    // user cannot access the parent directory
    write_log("myfs_link - EACCES\n");
    return -EACCES;

  } else if (result == MYFS_FIND_FOUND) {
    // there is already a file on this path that would be replaced
    write_log("myfs_link - EEXIST\n");
    return -EEXIST;

  } else {
    // get the link file name from the path and add the link to the parent directory
    // path string needs to be duplicated because it will be modified by path_file_name
    char* to_path_dup = strdup(to);
    char* file_name = path_file_name(to_path_dup);
    result = link_file(&dir_fcb, &from_fcb, file_name);
    free(to_path_dup);

    if (result < 0) {
      // the parent directory does not have space left to add the link
      write_log("myfs_link - EFBIG\n");
      return -EFBIG;
    }

    return 0;
  }
}

// Rename the file.
// Read 'man 2 rename'
static int myfs_rename(const char* from, const char* to) {
  write_log("myfs_rename(from=\"%s\", to=\"%s\")\n", from, to);
  int result;

  // try to find the renamed file
  struct my_fcb from_dir;
  struct my_fcb from_file;
  result = find_dir_entry(from, get_context_user(), &from_dir, &from_file);

  if (
    result == MYFS_FIND_NO_ACCESS ||
    !can_write(&from_dir, get_context_user())
  ) {
    // user cannot access or write to the original parent directory
    write_log("myfs_rename - EACCES\n");
    return -EACCES;
  } else if (result != MYFS_FIND_FOUND) {
    // renamed file does not exist
    write_log("myfs_rename - ENOENT\n");
    return -ENOENT;
  }

  // try to find the file that would be replaced
  struct my_fcb to_dir;
  struct my_fcb to_file;
  result = find_dir_entry(to, get_context_user(), &to_dir, &to_file);

  if (result == MYFS_FIND_NO_DIR) {
    // parent directory does not exist
    write_log("myfs_rename - ENOENT\n");
    return -ENOENT;
  }

  if (
    result == MYFS_FIND_NO_ACCESS ||
    !can_write(&to_dir, get_context_user())
  ) {
    // user cannot access or write to the destination parent directory
    write_log("myfs_rename - EACCES\n");
    return -EACCES;
  }

  // get the file names of the original and renamed files
  char* to_dup = strdup(to);
  char* to_file_name = path_file_name(to_dup);

  char* from_dup = strdup(from);
  char* from_file_name = path_file_name(from_dup);

  // if there is already a file at the destination, remove it
  if (result == MYFS_FIND_FOUND) {
    unlink_file(&to_dir, &to_file, to_file_name);
  }

  // remove the file from original directory and add it to the destination directory
  if (uuid_compare(from_dir.id, to_dir.id) == 0) {
    // if these directories are the same, it means that both from_dir and to_dir
    // contain the same FCB, but if we change one, the other one does not change
    // we need to make sure to use only one of the FCBs in this case
    remove_dir_entry(&from_dir, from_file_name);
    result = add_dir_entry(&from_dir, &from_file, to_file_name);
  } else {
    remove_dir_entry(&from_dir, from_file_name);
    result = add_dir_entry(&to_dir, &from_file, to_file_name);
  }

  free(to_dup);
  free(from_dup);

  if (result < 0) {
    // unable to add directory entry because the directory is too big
    write_log("myfs_rename - EFBIG\n");
    return -EFBIG;
  }

  return 0;
}

// Open a file. Open should check if the operation is permitted for the given flags (fi->flags).
// Read 'man 2 open'.
static int myfs_open(const char *path, struct fuse_file_info *fi){
  write_log("myfs_open(path\"%s\", fi=0x%08x)\n", path, fi);

  struct my_user user = get_context_user();
  struct my_fcb file_fcb;

  // try to find the file by its path
  int result = find_file(path, user, &file_fcb);

  if (
    result == MYFS_FIND_NO_DIR ||
    result == MYFS_FIND_NO_FILE ||
    !is_file(&file_fcb)
  ) {
    // the file does not exist or is not a file
    write_log("myfs_open - ENOENT\n");
    return -ENOENT;

  } else if (
    result == MYFS_FIND_NO_ACCESS ||
    !check_open_flags(&file_fcb, user, fi->flags)
  ) {
    // user cannot access the parent directory, or the opened file with the
    // specified flags
    write_log("myfs_open - EACCES\n");
    return -EACCES;
  }

  // open the file after creating it
  int fh = add_open_file(&file_fcb);

  if (fh < 0) {
    // too many files are open
    write_log("myfs_open - ENFILE\n");
    return -ENFILE;
  }

  // save the file handle for future calls
  fi->fh = fh;

  return 0;
}

// Release the file. There will be one call to release for each call to open.
static int myfs_release(const char *path, struct fuse_file_info *fi){
  write_log("myfs_release(path=\"%s\", fi=0x%08x)\n", path, fi);

  // remove the file from the open file table
  // if there are no other links pointing to it and is not open anywhere else,
  // it will be deleted
  remove_open_file(fi->fh);

  return 0;
}

static int myfs_opendir(const char *path, struct fuse_file_info *fi){
  write_log("myfs_opendir(path\"%s\", fi=0x%08x)\n", path, fi);

  struct my_user user = get_context_user();
  struct my_fcb dir_fcb;

  // try to find the directory by its path
  int result = find_file(path, user, &dir_fcb);

  if (
    result == MYFS_FIND_NO_DIR ||
    result == MYFS_FIND_NO_FILE ||
    !is_directory(&dir_fcb)
  ) {
    // the directory does not exist, or is not a directory
    write_log("myfs_open - ENOENT\n");
    return -ENOENT;

  } else if (
    result == MYFS_FIND_NO_ACCESS ||
    !check_open_flags(&dir_fcb, user, fi->flags)
  ) {
    // the user cannot access a parent directory, or cannot open the directory
    // wit the specified flags
    write_log("myfs_open - EACCES\n");
    return -EACCES;
  }

  // add the directory to the open file table
  int fh = add_open_file(&dir_fcb);

  if (fh < 0) {
    // too many files are open
    write_log("myfs_opendir - ENFILE\n");
    return -ENFILE;
  }

  // save the file for future calls
  fi->fh = fh;

  return 0;
}

// Release the directory. There will be one call to releasedir for each call to opendir.
static int myfs_releasedir(const char *path, struct fuse_file_info *fi){
  write_log("myfs_release(path=\"%s\", fi=0x%08x)\n", path, fi);

  // remove the directory from the open file table
  // if there are no other links pointing to it and is not open anywhere else,
  // it will be deleted
  remove_open_file(fi->fh);

  return 0;
}

static struct fuse_operations myfs_oper = {
  .getattr = myfs_getattr,
  .readdir = myfs_readdir,
  .open = myfs_open,
  .opendir = myfs_opendir,
  .read = myfs_read,
  .create = myfs_create,
  .utime = myfs_utime,
  .write = myfs_write,
  .truncate = myfs_truncate,
  .release = myfs_release,
  .releasedir = myfs_releasedir,
  .unlink = myfs_unlink,
  .mkdir = myfs_mkdir,
  .rmdir = myfs_rmdir,
  .chmod = myfs_chmod,
  .chown = myfs_chown,
  .link = myfs_link,
  .rename = myfs_rename,
};

void read_db_object(uuid_t key, void* buffer, size_t size) {
  // separate variable for size is required because unqlite will store
  // the read size in it
  unqlite_int64 unqlite_size = size;
  int rc = unqlite_kv_fetch(pDb, key, KEY_SIZE, buffer, &unqlite_size);
  error_handler(rc);
}

void write_db_object(uuid_t key, void* buffer, size_t size) {
  int rc = unqlite_kv_store(pDb, key, KEY_SIZE, buffer, size);
  error_handler(rc);
}

void delete_db_object(uuid_t key) {
  int rc = unqlite_kv_delete(pDb, key, KEY_SIZE);
  error_handler(rc);
}

char has_db_object(uuid_t key) {
  unqlite_int64 unqlite_size;
  // NULL is used as the buffer to prevent actually reading the object
  int rc = unqlite_kv_fetch(pDb, key, KEY_SIZE, NULL, &unqlite_size);

  if (rc == UNQLITE_OK) {
    return 1;
  } else if (rc == UNQLITE_NOTFOUND) {
    return 0;
  } else {
    error_handler(rc);
  }
}

void create_directory(mode_t mode, struct my_user user, struct my_fcb *dir_fcb) {
  dir_fcb->uid = user.uid;
  dir_fcb->gid = user.gid;
  dir_fcb->mode = S_IFDIR|mode;
  dir_fcb->mtime = time(0);
  dir_fcb->ctime = time(0);
  dir_fcb->size = 0;
  dir_fcb->nlink = 0;

  // create uuids for the FCB and index block
  uuid_generate(dir_fcb->id);
  uuid_generate(dir_fcb->data);

  // write the FCB to the database
  write_db_object(dir_fcb->id, dir_fcb, sizeof(struct my_fcb));

  // create an empty index block and write it to the database
  struct my_index index_block;
  write_db_object(dir_fcb->data, &index_block, sizeof(index_block));

  // create an empty directory header and write it to the database
  struct my_dir_header dir_header = {0, -1};
  write_file_data(dir_fcb, &dir_header, sizeof(dir_header), 0);
}

void create_file(mode_t mode, struct my_user user, struct my_fcb* file_fcb) {
  file_fcb->uid = user.uid;
  file_fcb->gid = user.gid;
  file_fcb->mode = S_IFREG|mode;
  file_fcb->mtime = time(0);
  file_fcb->ctime = time(0);
  file_fcb->size = 0;
  file_fcb->nlink = 0;

  // create uuids for the FCB and index block
  uuid_generate(file_fcb->id);
  uuid_generate(file_fcb->data);

  // write the FCB to the database
  write_db_object(file_fcb->id, file_fcb, sizeof(struct my_fcb));

  // create an empty index block and write it to the database
  struct my_index index_block;
  write_db_object(file_fcb->data, &index_block, sizeof(index_block));
}

int read_file(uuid_t *id, struct my_fcb* file_fcb) {
  if (has_db_object(*id)) {
    read_db_object(*id, file_fcb, sizeof(struct my_fcb));
    return 0;

  } else {
    return -1;
  }
}

void update_file(struct my_fcb* file_fcb) {
  write_db_object(file_fcb->id, file_fcb, sizeof(struct my_fcb));
}

size_t size_round_up_to(size_t num, size_t up_to) {
  size_t rem = num % up_to;

  if (rem > 0) {
    // add to the number so that it is a multiple of up_to
    return num + (up_to - rem);
  } else {
    return num;
  }
}

size_t size_round_down_to(size_t num, size_t up_to) {
  size_t rem = num % up_to;

  // subtract from the number so that it is a multiple of up_to
  return num - rem;
}

int get_num_blocks(size_t size) {
  return size_round_up_to(size, MY_BLOCK_SIZE) / MY_BLOCK_SIZE;
}

void get_block_indexes(size_t size, off_t offset, int* first, int* last) {
  *first = size_round_down_to(offset, MY_BLOCK_SIZE) / MY_BLOCK_SIZE;
  *last = size_round_up_to(offset + size, MY_BLOCK_SIZE) / MY_BLOCK_SIZE - 1;
}

void remove_file(struct my_fcb* file_fcb) {
  // read the index block for the file
  struct my_index index_block;
  read_db_object(file_fcb->data, &index_block, sizeof(index_block));

  // get the number of data blocks used by the file data
  int num_blocks = get_num_blocks(file_fcb->size);

  // delete all data blocks
  for (int block = 0; block < num_blocks; block++) {
    delete_db_object(index_block.entries[block]);
  }

  // delete the index block and FCB
  delete_db_object(file_fcb->data);
  delete_db_object(file_fcb->id);
}

void truncate_file(struct my_fcb* file_fcb, size_t size) {
  // read the index block for the file
  struct my_index index_block;
  read_db_object(file_fcb->data, &index_block, sizeof(index_block));

  /** @var Number of data blocks currently used by the file */
  int old_num_blocks = get_num_blocks(file_fcb->size);

  /** @var Number of data blocks the file needs to have */
  int new_num_blocks = get_num_blocks(size);

  if (new_num_blocks > old_num_blocks) {
    // we need to create some blocks at the end of the file

    /** @var Pointer to a data block filled with zeroes */
    void* empty_block = calloc(1, MY_BLOCK_SIZE);

    // go through all data blocks we need to create
    for (int block = old_num_blocks; block < new_num_blocks; block++) {
      // create UUID for the data block and write an empty data block into the database
      uuid_generate(index_block.entries[block]);
      write_db_object(index_block.entries[block], empty_block, MY_BLOCK_SIZE);
    }

    free(empty_block);

    // save changes made in the index block to the database
    write_db_object(file_fcb->data, &index_block, sizeof(index_block));

  } else if (new_num_blocks < old_num_blocks) {
    // we need to remove some blocks at the end of the file

    // go through all data blocks that need to be removed
    for (int block = new_num_blocks; block < old_num_blocks; block++) {
      delete_db_object(index_block.entries[block]);
    }
  }

  // finally update file size and modification time
  file_fcb->size = size;
  file_fcb->mtime = time(0);
  update_file(file_fcb);
}

void read_block_to_buffer(uuid_t id, int block_num, void* buffer, size_t size, off_t offset) {
  // read the data from the data block into memory
  void* block_data = malloc(MY_BLOCK_SIZE);
  read_db_object(id, block_data, MY_BLOCK_SIZE);

  /** @var Offset in the file of the first byte of the block */
  off_t block_start = block_num * MY_BLOCK_SIZE;
  /** @var Offset in the file of the last byte of the block */
  off_t block_end = block_start + MY_BLOCK_SIZE - 1;

  /** @var Offset in the file of the first byte of the read data */
  off_t data_start = offset;
  /** @var Offset in the file of the last byte of the read data */
  off_t data_end = offset + size - 1;

  // requested data range is fully contained in the block
  if (block_start <= data_start && block_end >= data_end) {
    // copy a slice of the data block into the buffer
    memcpy(buffer, block_data + (data_start - block_start), size);
  }

  // requested data range starts in the block, but ends outside it
  if (block_start <= data_start && block_end < data_end) {
    // copy the data from data start until the block end to the buffer
    memcpy(buffer, block_data + (data_start - block_start), block_end - data_start + 1);
  }

  // request data range ends in the block, but starts outside it
  if (block_start > data_start && block_end >= data_end) {
    // copy the data from block start until the data end to the buffer
    memcpy(buffer + (block_start - data_start), block_data, data_end - block_start + 1);
  }

  // requested data range starts and ends outside the block
  if (block_start > data_start && block_end < data_end) {
    // copy the whole data block to the buffer
    memcpy(buffer + (block_start - data_start), block_data, MY_BLOCK_SIZE);
  }

  free(block_data);
}

void read_file_data(struct my_fcb* file_fcb, void* buffer, size_t size, off_t offset) {
  // read the index block from the database
  struct my_index index_block;
  read_db_object(file_fcb->data, &index_block, sizeof(index_block));

  // get the indexes of the first and last data block needed for reading
  int first_block, last_block;
  get_block_indexes(size, offset, &first_block, &last_block);

  // go through the data blocks and read data from them into the buffer
  for (int block = first_block; block <= last_block; block++) {
    read_block_to_buffer(index_block.entries[block], block, buffer, size, offset);
  }
}

void write_buffer_to_block(uuid_t id, int block_num, void* buffer, size_t size, off_t offset) {
  // read the data from the data block into memory
  // we need to do this because we will be writing the whole block back into
  // database, even though only a part of it may change
  void* block_data = malloc(MY_BLOCK_SIZE);
  read_db_object(id, block_data, MY_BLOCK_SIZE);

  /** @var Offset in the file of the first byte of the block */
  off_t block_start = block_num * MY_BLOCK_SIZE;
  /** @var Offset in the file of the last byte of the block */
  off_t block_end = block_start + MY_BLOCK_SIZE - 1;

  /** @var Offset in the file of the first byte of the read data */
  off_t data_start = offset;
  /** @var Offset in the file of the last byte of the read data */
  off_t data_end = offset + size - 1;

  // needed data range is fully contained in the block
  if (block_start <= data_start && block_end >= data_end) {
    // copy data from the whole buffer into the data block
    memcpy(block_data + (data_start - block_start), buffer, size);
  }

  // needed data range starts in the block, but ends outside it
  if (block_start <= data_start && block_end < data_end) {
    // copy data from the start of the buffer into the data block
    memcpy(block_data + (data_start - block_start), buffer, block_end - data_start + 1);
  }

  // needed data range ends in the block, but starts outside it
  if (block_start > data_start && block_end >= data_end) {
    // copy data from the end of the buffer into the data block
    memcpy(block_data, buffer + (block_start - data_start), data_end - block_start + 1);
  }

  // needed data range starts and ends outside the block
  if (block_start > data_start && block_end < data_end) {
    // copy data from the block into the whole data block
    memcpy(block_data, buffer + (block_start - data_start), MY_BLOCK_SIZE);
  }

  // save the changed data in the block back to the database
  write_db_object(id, block_data, MY_BLOCK_SIZE);
  free(block_data);
}

void write_file_data(struct my_fcb* file_fcb, void* buffer, size_t size, off_t offset) {
  // if we are writing outside the current file data, it needs to be expanded
  // first to be able to accommodate the new data
  if ((offset + size) > file_fcb->size) {
    truncate_file(file_fcb, offset + size);
  }

  // read the index block from the database
  struct my_index index_block;
  read_db_object(file_fcb->data, &index_block, sizeof(index_block));

  // get the indexes of the first and last data block needed for writing
  int first_block, last_block;
  get_block_indexes(size, offset, &first_block, &last_block);

  // go through the data blocks and write data to them from the buffer
  for (int block = first_block; block <= last_block; block++) {
    write_buffer_to_block(index_block.entries[block], block, buffer, size, offset);
  }

  // finally update modification time
  file_fcb->mtime = time(0);
  update_file(file_fcb);
}

struct my_dir_entry* get_dir_entry(void* dir_data, int offset) {
  return dir_data + sizeof(struct my_dir_header) + offset * sizeof(struct my_dir_entry);
}

int add_dir_entry(struct my_fcb* dir_fcb, struct my_fcb* file_fcb, const char* name) {
  /** @var Size of the directory data */
  size_t data_size = dir_fcb->size;
  /** @var Size of the data if there is a new entry created */
  size_t max_size = data_size + sizeof(struct my_dir_entry);

  // load the directory data from the database
  // buffer is big enough to add a new directory entry if needed
  void* dir_data = malloc(max_size);
  read_file_data(dir_fcb, dir_data, data_size, 0);

  struct my_dir_header* dir_header = dir_data;

  /** @var Directory entry that is not currently used */
  struct my_dir_entry* free_entry;

  if (dir_header->first_free > -1) {
    // there is an unused entry in the directory, we can use it
    free_entry = get_dir_entry(dir_data, dir_header->first_free);

    // update the free list header to point to the next free entry
    dir_header->first_free = free_entry->next_free;
  } else {
    // there are no free, unused entries in the directory

    // check if we are able to increase the file size
    if (max_size > MY_MAX_FILE_SIZE) {
      // no space left for the new entry
      free(dir_data);
      return -1;
    }

    // create a new entry at the end
    free_entry = get_dir_entry(dir_data, dir_header->items);

    // update the number of entries and directory data size
    dir_header->items++;
    data_size = max_size;
  }

  // copy the entry name and file UUID into the entry
  strncpy(free_entry->name, name, MY_MAX_PATH - 1);
  uuid_copy(free_entry->fcb_id, file_fcb->id);

  // mark the entry as used
  free_entry->used = 1;

  // write changed directory data back to the database
  write_file_data(dir_fcb, dir_data, data_size, 0);

  free(dir_data);

  return 0;
}

int remove_dir_entry(struct my_fcb* dir_fcb, const char* name) {
  // load the directory data from the database
  void* dir_data = malloc(dir_fcb->size);
  read_file_data(dir_fcb, dir_data, dir_fcb->size, 0);

  struct my_dir_header* dir_header = dir_data;

  /** @var Directory entry to be removed */
  struct my_dir_entry* dir_entry;

  char found = 0;
  int offset;

  // find the entry to remove by the entry name
  for (offset = 0; offset < dir_header->items; offset++) {
    dir_entry = get_dir_entry(dir_data, offset);

    // skip unused entries
    if (!dir_entry->used) continue;

    if (strcmp(dir_entry->name, name) == 0) {
      found = 1;
      break;
    }
  }

  if (found) {
    // clear the directory entry, this also sets the used field to 0
    memset(dir_entry, 0, sizeof(struct my_dir_entry));

    // add the directory entry to the start of the free list
    dir_entry->next_free = dir_header->first_free;
    dir_header->first_free = offset;

    // write directory data back to the database
    write_file_data(dir_fcb, dir_data, dir_fcb->size, 0);

    return 0;
  } else {
    return -1;
  }
}

void iterate_dir_entries(struct my_fcb* dir_fcb, struct my_dir_iter* iter) {
  iter->position = 0;

  // load the directory data from the database
  iter->dir_data = malloc(dir_fcb->size);
  read_file_data(dir_fcb, iter->dir_data, dir_fcb->size, 0);
}

struct my_dir_entry* next_dir_entry(struct my_dir_iter* iter) {
  struct my_dir_header* dir_header = iter->dir_data;

  // start to go through all the remaining directory entries
  while (iter->position < dir_header->items) {
    struct my_dir_entry* entry = get_dir_entry(iter->dir_data, iter->position);

    iter->position++;

    // we found an used entry, return it
    if (entry->used) {
      return entry;
    }
  }

  // there no used entries left
  return NULL;
}

void clean_dir_iterator(struct my_dir_iter* iter) {
  free(iter->dir_data);
}

int get_directory_size(struct my_fcb* dir_fcb) {
  struct my_dir_iter iter;
  iterate_dir_entries(dir_fcb, &iter);

  int dir_size = 0;
  struct my_dir_entry* entry;

  // go through all directory entries and increase counter if we find an used entry
  while ((entry = next_dir_entry(&iter)) != NULL) {
    if (entry->used) dir_size++;
  }

  clean_dir_iterator(&iter);

  return dir_size;
}

int link_file(struct my_fcb* dir_fcb, struct my_fcb* file_fcb, const char* name) {
  // add file as an entry to the directory
  int result = add_dir_entry(dir_fcb, file_fcb, name);

  if (result < 0) {
    return -1;
  }

  // update number of links pointing to the file
  file_fcb->nlink++;
  update_file(file_fcb);

  return 0;
}

void unlink_file(struct my_fcb* dir_fcb, struct my_fcb* file_fcb, const char* name) {
  // remove the entry for the file from the directory
  remove_dir_entry(dir_fcb, name);

  // there are some links pointing to the file, remove one
  if (file_fcb->nlink > 0) {
    file_fcb->nlink--;
    update_file(file_fcb);
  }

  // if no links point to the file and it is not open, delete it
  if (file_fcb->nlink == 0 && !is_file_open(file_fcb)) {
    remove_file(file_fcb);
  }
}

int find_file(const char* path, struct my_user user, struct my_fcb* file_fcb) {
  // use find_dir_entry but do not use the found directory entry
  struct my_fcb dir_fcb;
  return find_dir_entry(path, user, &dir_fcb, file_fcb);
}

int find_dir_entry(const char* const_path, struct my_user user, struct my_fcb* dir_fcb, struct my_fcb* file_fcb) {
  // read FCB of the root directory
  struct my_fcb root_dir;
  read_file(&(root_object.id), &root_dir);

  // we start at the root directory
  // if the path is '/' it will be returned as the file
  memcpy(file_fcb, &root_dir, sizeof(struct my_fcb));

  // duplicate the path string so that we can modify it
  // but store the original refernce so that we can free it later
  char* full_path = strdup(const_path);
  char* path = full_path;

  // put the first path component into entry_name
  // change path to contain the rest of the path
  char* entry_name = path_split(&path);

  // if entry_name is NULL, it means we have exhausted the path and got to the
  // requested file
  // if it is NULL, it means we need to continue the tree traversal further
  while (entry_name != NULL) {
    // the previously loaded file is expected to be a directory
    // if it isn't, it means some directory in the path does not exist
    if (!is_directory(file_fcb)) {
      free(full_path);
      return MYFS_FIND_NO_DIR;
    }

    // we need to check permissions while traversing the tree
    // user needs to have execute permissions to access the directory entries
    if (!can_execute(file_fcb, user)) {
      free(full_path);
      return MYFS_FIND_NO_ACCESS;
    }

    // we made sure this is a directory, copy it to dir_entry in case it's
    // a parent directory of the file we're looking for
    memcpy(dir_fcb, file_fcb, sizeof(struct my_fcb));

    // iterate directory entries
    struct my_dir_iter iter;
    iterate_dir_entries(dir_fcb, &iter);

    struct my_dir_entry* entry;
    char found = 0;

    // go through the entries until we find a match
    while ((entry = next_dir_entry(&iter)) != NULL) {
      if (strcmp(entry_name, entry->name) == 0) {
        found = 1;
        break;
      }
    }

    // a match is found, read it into file_fcb and get to the next path component
    // if this was the last component in the path, the while loop will stop and
    // function will return the found file in file_fcb and its parent directory
    // in dir_fcb
    if (found) {
      read_file(&(entry->fcb_id), file_fcb);
      entry_name = path_split(&path);
    }

    clean_dir_iterator(&iter);

    // if directory entry does not exist...
    if (!found) {
      free(full_path);
      // ... check if there were any other components remaining in the path
      // if yes, it means that we did not find a parent directory
      // otherwise we didn't find the file (last component of the path)
      return (path == NULL) ? MYFS_FIND_NO_FILE : MYFS_FIND_NO_DIR;
    }
  }

  free(full_path);
  return MYFS_FIND_FOUND;
}

char* path_split(char** path) {
  // strsep will modify path to point to the string after '/'
  // it will return the original value of path, i.e. string before '/'
  char* head = strsep(path, "/");

  if (head == NULL) {
    // path was empty
    return NULL;

  } else if (*head == '\0') {
    // the first character of the string was '/'
    // e.g. /bar -> head == "", bar == "bar"

    if (path != NULL) {
      // there were more characters after the '/'
      // ignore head and re-run the function
      return path_split(path);
    } else {
      // this was the last character in the path
      return NULL;
    }

  } else {
    // path started with a valid component
    return head;
  }
}

char* path_file_name(char* path) {
  // keep splitting the path until we find the last component (file name)
  char* file_name = path_split(&path);

  while (path != NULL) {
    char* next_file_name = path_split(&path);

    // if the last character in the path is '/', next_file_name will be NULL
    // in this case we need to ignore the NULL
    if (next_file_name != NULL) {
      file_name = next_file_name;
    }
  }

  return file_name;
}

char is_directory(struct my_fcb* fcb) {
  return (fcb->mode & S_IFDIR) == S_IFDIR;
}

char is_file(struct my_fcb* fcb) {
  return (fcb->mode & S_IFREG) == S_IFREG;
}

char has_permission(struct my_fcb* fcb, struct my_user user,
    mode_t user_mode, mode_t group_mode, mode_t other_mode) {
  if (fcb->uid == user.uid) {
    // user is owner of the file, check user permissions
    return (fcb->mode & user_mode) == user_mode;
  } else if (fcb->gid == user.gid) {
    // user's group is owner of the file, check group permissions
    return (fcb->mode & group_mode) == group_mode;
  } else {
    // user or group isn't owner, check others permissions
    return (fcb->mode & other_mode) == other_mode;
  }
}

struct my_user get_context_user() {
  // copy UID and GID from FUSE context into the user struct
  struct fuse_context* context = fuse_get_context();

  struct my_user user = {
    .uid = context->uid,
    .gid = context->gid,
  };

  return user;
}

char can_read(struct my_fcb* fcb, struct my_user user) {
  // check user, group or other read permissions as appropriate
  return has_permission(fcb, user, S_IRUSR, S_IRGRP, S_IROTH);
}

char can_write(struct my_fcb* fcb, struct my_user user) {
  // check user, group or other write permissions as appropriate
  return has_permission(fcb, user, S_IWUSR, S_IWGRP, S_IWOTH);
}

char can_execute(struct my_fcb* fcb, struct my_user user) {
  // check user, group or other execute permissions as appropriate
  return has_permission(fcb, user, S_IXUSR, S_IXGRP, S_IXOTH);
}

char check_open_flags(struct my_fcb* fcb, struct my_user user, int flags) {
  if (flags & O_RDWR) {
    // read-write
    return can_read(fcb, user) && can_write(fcb, user);
  } else if (flags & O_WRONLY) {
    // write-only
    return can_write(fcb, user);
  } else {
    // read-only
    return can_read(fcb, user);
  }
}

int get_free_file_handle() {
  // go through open file entries and look for an unused one
  for (int fh = 0; fh < MY_MAX_OPEN_FILES; fh++) {
    if (!open_files[fh].used) {
      return fh;
    }
  }

  // no unused entry found, too many files are open
  return -1;
}

int get_open_file(int fh, struct my_fcb* fcb) {
  // is the file handle actually valid?
  if (open_files[fh].used) {
    read_db_object(open_files[fh].id, fcb, sizeof(struct my_fcb));
    return 0;
  } else {
    return -1;
  }
}

int add_open_file(struct my_fcb* file) {
  int fh = get_free_file_handle();

  if (fh != -1) {
    // able to open another file, save it's UUID and mark the entry as used
    uuid_copy(open_files[fh].id, file->id);
    open_files[fh].used = 1;

    return fh;
  } else {
    // too many files are open
    return -1;
  }
}

int remove_open_file(int fh) {
  struct my_fcb file;

  // find the FCB for the file handle
  if (get_open_file(fh, &file) == 0) {
    open_files[fh].used = 0;

    // the file was removed while it was open, remove it if it isn't open anywhere else
    if (file.nlink == 0 && !is_file_open(&file)) {
      remove_file(&file);
    }

    return 0;
  } else {
    return -1;
  }
}

char is_file_open(struct my_fcb* file) {
  // go through open file entries and look for one with a matching UUID
  for (int fh = 0; fh < MY_MAX_OPEN_FILES; fh++) {
    if (open_files[fh].used && uuid_compare(open_files[fh].id, file->id) == 0) {
      return 1;
    }
  }

  // no matching entry found, file is not open
  return 0;
}

// Initialise the in-memory data structures from the store. If the root object (from the store) is empty then create a root fcb (directory)
// and write it to the store. Note that this code is executed outide of fuse. If there is a failure then we have failed toi initlaise the
// file system so exit with an error code.
void init_fs(){
  printf("init_fs\n");

  // Initialise the store.
  init_store();

  if (root_is_empty) {
    printf("init_fs: root is empty\n");

    printf("init_fs: creating root directory\n");

    // cannot user FUSE context because it has not been initialised yet
    struct my_user user = {.uid = getuid(), .gid = getgid()};

    // create the root directory
    struct my_fcb root_dir_fcb;
    create_directory(S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH,
      user, &root_dir_fcb);

    // normally the parent directory points to the child directory, but root
    // directory has no parents
    // set the number of links to 1 manually to keep it from being deleted
    // after closing
    root_dir_fcb.nlink = 1;
    update_file(&root_dir_fcb);

    printf("init_fs: writing updated root object\n");

    // save the root object to the database
    uuid_copy(root_object.id, root_dir_fcb.id);
    int rc = write_root();

     if (rc != UNQLITE_OK) {
       error_handler(rc);
    }
  }
}

void shutdown_fs(){
  unqlite_close(pDb);
}

int main(int argc, char *argv[]){
  int fuserc;
  struct myfs_state *myfs_internal_state;

  //Setup the log file and store the FILE* in the private data object for the file system.
  myfs_internal_state = malloc(sizeof(struct myfs_state));
  myfs_internal_state->logfile = init_log_file();

  //Initialise the file system. This is being done outside of fuse for ease of debugging.
  init_fs();

  fuserc = fuse_main(argc, argv, &myfs_oper, myfs_internal_state);

  //Shutdown the file system.
  shutdown_fs();

  return fuserc;
}
