% CS3104 P2 Report
% 140015533
% 14 November 2016

# Overview

In this practical my task was to implement a simple userspace file system using FUSE. The file system uses UnQLite for storing the file data and meta-data.

The file system has a tree directory hierarchy where directories can contain both files and directories. Each file and directory has standard meta-data attached to it. Files can be created, updated and deleted. Files have dynamic size up to a certain limit. Hard links and renaming files is supported.

# Design and implementation

## Files

All regular files and directories are treated as files by the file system. Each file has an UUID assigned to it. This UUID is then used as a key in the database.

### Meta-data

Each file carries some meta-data. In my implementation these are:

* User ID
* Group ID
* File mode (permissions)
* Time of last access, modification and change to meta-data
* Number of links pointing to it
* Size of the file in bytes

These are similar to the fields in the standard Linux stat struct. All meta-data is stored in the file control block.

### Data

File data is stored separately from the meta-data. It is split into a number of data blocks of fixed size. This means that file data can be read and written without accessing the whole data at a time, but only small data blocks one at a time.

Each data block can be accessed by its UUID. These UUIDs are stored in an index block, which contains a fixed-size table mapping data block number to its UUID. Because of that there is a limit on file size, which depends on the size of the data block and number of entries in the index block.

### Creating, updating and deleting

When creating a file, its UUID is generated and meta-data (UID, GID, mode) initialised to specified values in the file control block. The FCB is then stored in the database. Additionaly, the UUID of the index block of the file is generated. An empty index block is then created and stored in the database.

To update the file meta-data, the FCB struct is modified and immediately stored in the database, replacing the old file control block.

File is deleted by removing all the data blocks, the index block and the file control block from the database. There cannot be any links pointing to the file when it is being deleted, or the file system will not function properly.

## Persistent storage data

All persistent data is stored in an UnQLite database. UUIDs from libuuid are used as keys for all blocks. One exception is the root object, which only contains the UUID of the root directory file control block.

### Database access

I have created helper functions that are used to read, write and delete UnQLite objects. They always use UUIDs as keys and handle error return values from the database. This allows me to change the database implementation if needed without modifying other code. It also simplifies other methods by hiding the database API.

### Data structures

![Diagram of the data structures used for storing files and their data \label{fig:ondiskstructs}](figures/ondiskstructs.png)

### Reading and writing

### Truncating

### Removing

## Directories

### Creating

### Data structures

### Iterating entries

### Adding and removing entries

### Renaming

## Tree traversal

## Open files

## Permissions

# Testing

## Test programs

## Large files

# Evaluation

## System limits

# Conclusion
