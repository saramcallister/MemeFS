/*
  FUSE: Filesystem in Userspace
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>

  This program can be distributed under the terms of the GNU GPL.
  See the file COPYING.

  gcc -Wall `pkg-config fuse --cflags --libs` fusememe.c -o fusememe
*/

#define FUSE_USE_VERSION 26

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef linux
/* For pread()/pwrite() */
#define _XOPEN_SOURCE 500
#define _GNU_SOURCE
#endif

#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <sys/time.h>
#include <unistd.h>
#ifdef HAVE_SETXATTR
#include <sys/xattr.h>
#endif

#include "blocklayer.h"

#define NUM_DIRECT_BLOCKS 6
#define NUM_SINGLY_INDIRECT_BLOCKS 1
#define NUM_DOUBLY_INDIRECT_BLOCKS 1
#define MAGIC_NUMBER 0x1337
#define MAX_NAME_SIZE 32

typedef struct {
	size_t direct[NUM_DIRECT_BLOCKS];
	size_t singly_indirect[NUM_SINGLY_INDIRECT_BLOCKS];
	size_t doubly_indirect[NUM_DOUBLY_INDIRECT_BLOCKS];
} block_list;

typedef struct {
	mode_t mode;
	size_t inode_number;
	size_t size;
	block_list data;
	char padding[36];	
} inode;

typedef struct {
	size_t inode_number;
	char name[MAX_NAME_SIZE];
} dir_entry;

typedef struct {
	inode root_inode;
	size_t current_size;
	size_t magic_number;
	size_t num_inodes_used;
	size_t inode_block;
} meme_superblock;

static char *current_path; /* Path starting directory */
static meme_superblock superblock;
static inode inode_table[BLOCKSIZE/sizeof(inode)];

block_list zero_block_list() 
{
	size_t direct[NUM_DIRECT_BLOCKS] = {0};
	size_t singly_indirect[NUM_SINGLY_INDIRECT_BLOCKS] = {0};
	size_t doubly_indirect[NUM_DOUBLY_INDIRECT_BLOCKS] = {0};
	block_list zeroed_data;

	memcpy(zeroed_data.direct, direct, sizeof(direct));
	memcpy(zeroed_data.singly_indirect, singly_indirect, sizeof(singly_indirect));
	memcpy(zeroed_data.doubly_indirect, doubly_indirect, sizeof(doubly_indirect));
	return zeroed_data;
}

void *meme_init(struct fuse_conn_info *conn)
{
	char buf[BLOCKSIZE];
	inode root_inode;
	dir_entry default_files;

	/* Initialize block layer */
	block_dev_init(current_path);

	/* Read superblock */
	read_block(0, buf);
	memcpy(&superblock, buf, sizeof(meme_superblock));
	
	/* check if file system already exists */
	if (superblock.magic_number == MAGIC_NUMBER) {
		/* read inode table */
		read_block(superblock.inode_block, buf);
		memcpy(inode_table, buf, BLOCKSIZE);
		return NULL;
	}

	/* if file system does not already exist, create superblock */
	allocate_block(); // save first block for superblock
	root_inode.mode = S_IFDIR | 700;
	root_inode.inode_number = 0;
	root_inode.size = 2*sizeof(dir_entry);
	root_inode.data = zero_block_list();
	root_inode.data.direct[0] = allocate_block();

	superblock.magic_number = MAGIC_NUMBER;
	superblock.current_size = 2*BLOCKSIZE;
	superblock.root_inode = root_inode;
	superblock.num_inodes_used = 1;
	superblock.inode_block = allocate_block();

	/* initialize inode_table */
	inode_table[0] = root_inode;

	/* write '.' and '..' to root directory */
	default_files.inode_number = 0;
	strncpy(default_files.name, ".", MAX_NAME_SIZE);
	memcpy(buf, &default_files, sizeof(dir_entry));
	strncpy(default_files.name, "..", MAX_NAME_SIZE);
	memcpy(buf + sizeof(dir_entry), &default_files, sizeof(dir_entry));
	write_block(root_inode.data.direct[0], buf);

	return NULL;
}

void meme_destroy(void *private_data) {
	char buf[BLOCKSIZE];

	write_block(superblock.inode_block, (char *) inode_table);
	memcpy(buf, &superblock, sizeof(superblock));
	write_block(0, buf);

	block_dev_destroy();
	free(current_path);
}

/* Find inode location in inode table for given path,
 * Returns -1 if cannot be found */
int find_inode_loc(const char *path) 
{
	size_t inode_loc = 0;
	char *path_copy;
	char *token;
	size_t num_entries;
	dir_entry buf[BLOCKSIZE/sizeof(dir_entry)];
	size_t i;

	path_copy = (char *) malloc(strlen(path) + 1);
	strcpy(path_copy, path);

	token = strtok(path_copy, "/");
	while (token) {
		num_entries = inode_table[inode_loc].size / sizeof(dir_entry);
		// TODO: modify to work with more than 1 block's worth of dir_entries
		read_block(inode_table[inode_loc].data.direct[1], (char *) buf); 
		for (i = 0; i < num_entries; i++) {
			if (!strcmp(buf[i].name, token)) {
				break;
			} else if (i == num_entries - 1) {
				return -1; // File not found
			}
		}

		inode_loc = buf[i].inode_number;
		token = strtok(NULL, "/");
	}

	return inode_loc;
}

static int meme_getattr(const char *path, struct stat *stbuf)
{
	int res = 0;
	
	res = find_inode_loc(path);
	if (strcmp(path, "/") == 0) {
		stbuf->st_mode = superblock.root_inode.mode;
		stbuf->st_nlink = 2;
	} else if (res == -1) {
		return -ENOENT;
	} else {
		stbuf->st_mode = inode_table[res].mode;
		stbuf->st_nlink = 2;
	}
	return 0;
}

static int meme_access(const char *path, int mask)
{
	int res;

	res = find_inode_loc(path);
	if (res == -1){
		return -ENOENT;
	} else if (!(inode_table[res].mode & mask)) {
		return -EACCES;
	}

	return 0;
}

static int meme_readlink(const char *path, char *buf, size_t size)
{
	int res;

	res = readlink(path, buf, size - 1);
	if (res == -1)
		return -errno;

	buf[res] = '\0';
	return 0;
}


static int meme_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
		       off_t offset, struct fuse_file_info *fi)
{
	DIR *dp;
	struct dirent *de;

	(void) offset;
	(void) fi;

	dp = opendir(path);
	if (dp == NULL)
		return -errno;

	while ((de = readdir(dp)) != NULL) {
		struct stat st;
		memset(&st, 0, sizeof(st));
		st.st_ino = de->d_ino;
		st.st_mode = de->d_type << 12;
		if (filler(buf, de->d_name, &st, 0))
			break;
	}

	closedir(dp);
	return 0;
}

static int meme_mknod(const char *path, mode_t mode, dev_t rdev)
{
	int res;

	/* On Linux this could just be 'mknod(path, mode, rdev)' but this
	   is more portable */
	if (S_ISREG(mode)) {
		res = open(path, O_CREAT | O_EXCL | O_WRONLY, mode);
		if (res >= 0)
			res = close(res);
	} else if (S_ISFIFO(mode))
		res = mkfifo(path, mode);
	else
		res = mknod(path, mode, rdev);
	if (res == -1)
		return -errno;

	return 0;
}

static int meme_mkdir(const char *path, mode_t mode)
{
	int res;

	res = mkdir(path, mode);
	if (res == -1)
		return -errno;

	return 0;
}

static int meme_unlink(const char *path)
{
	int res;

	res = unlink(path);
	if (res == -1)
		return -errno;

	return 0;
}

static int meme_rmdir(const char *path)
{
	int res;

	res = rmdir(path);
	if (res == -1)
		return -errno;

	return 0;
}

static int meme_symlink(const char *to, const char *from)
{
	int res;

	res = symlink(to, from);
	if (res == -1)
		return -errno;

	return 0;
}

static int meme_rename(const char *from, const char *to)
{
	int res;

	res = rename(from, to);
	if (res == -1)
		return -errno;

	return 0;
}

static int meme_link(const char *from, const char *to)
{
	int res;

	res = link(from, to);
	if (res == -1)
		return -errno;

	return 0;
}

static int meme_chmod(const char *path, mode_t mode)
{
	int res;

	res = chmod(path, mode);
	if (res == -1)
		return -errno;

	return 0;
}

static int meme_chown(const char *path, uid_t uid, gid_t gid)
{
	int res;

	res = lchown(path, uid, gid);
	if (res == -1)
		return -errno;

	return 0;
}

static int meme_truncate(const char *path, off_t size)
{
	int res;

	res = truncate(path, size);
	if (res == -1)
		return -errno;

	return 0;
}

static int meme_utimens(const char *path, const struct timespec ts[2])
{
	int res;
	struct timeval tv[2];

	tv[0].tv_sec = ts[0].tv_sec;
	tv[0].tv_usec = ts[0].tv_nsec / 1000;
	tv[1].tv_sec = ts[1].tv_sec;
	tv[1].tv_usec = ts[1].tv_nsec / 1000;

	res = utimes(path, tv);
	if (res == -1)
		return -errno;

	return 0;
}

static int meme_open(const char *path, struct fuse_file_info *fi)
{
	int res;

	res = open(path, fi->flags);
	if (res == -1)
		return -errno;

	close(res);
	return 0;
}

static int meme_read(const char *path, char *buf, size_t size, off_t offset,
		    struct fuse_file_info *fi)
{
	int fd;
	int res;

	(void) fi;
	fd = open(path, O_RDONLY);
	if (fd == -1)
		return -errno;

	res = pread(fd, buf, size, offset);
	if (res == -1)
		res = -errno;

	close(fd);
	return res;
}

static int meme_write(const char *path, const char *buf, size_t size,
		     off_t offset, struct fuse_file_info *fi)
{
	int fd;
	int res;

	(void) fi;
	fd = open(path, O_WRONLY);
	if (fd == -1)
		return -errno;

	res = pwrite(fd, buf, size, offset);
	if (res == -1)
		res = -errno;

	close(fd);
	return res;
}

static int meme_statfs(const char *path, struct statvfs *stbuf)
{
	int res;

	res = statvfs(path, stbuf);
	if (res == -1)
		return -errno;

	return 0;
}

static int meme_release(const char *path, struct fuse_file_info *fi)
{
	/* Just a stub.	 This method is optional and can safely be left
	   unimplemented */

	(void) path;
	(void) fi;
	return 0;
}

static int meme_fsync(const char *path, int isdatasync,
		     struct fuse_file_info *fi)
{
	/* Just a stub.	 This method is optional and can safely be left
	   unimplemented */

	(void) path;
	(void) isdatasync;
	(void) fi;
	return 0;
}

#ifdef HAVE_SETXATTR
/* xattr operations are optional and can safely be left unimplemented */
static int meme_setxattr(const char *path, const char *name, const char *value,
			size_t size, int flags)
{
	int res = lsetxattr(path, name, value, size, flags);
	if (res == -1)
		return -errno;
	return 0;
}

static int meme_getxattr(const char *path, const char *name, char *value,
			size_t size)
{
	int res = lgetxattr(path, name, value, size);
	if (res == -1)
		return -errno;
	return res;
}

static int meme_listxattr(const char *path, char *list, size_t size)
{
	int res = llistxattr(path, list, size);
	if (res == -1)
		return -errno;
	return res;
}

static int meme_removexattr(const char *path, const char *name)
{
	int res = lremovexattr(path, name);
	if (res == -1)
		return -errno;
	return 0;
}
#endif /* HAVE_SETXATTR */

static struct fuse_operations meme_oper = {
	.getattr	= meme_getattr,
	.access		= meme_access,
	.readlink	= meme_readlink,
	.readdir	= meme_readdir,
	.mknod		= meme_mknod,
	.mkdir		= meme_mkdir,
	.symlink	= meme_symlink,
	.unlink		= meme_unlink,
	.rmdir		= meme_rmdir,
	.rename		= meme_rename,
	.link		= meme_link,
	.chmod		= meme_chmod,
	.chown		= meme_chown,
	.truncate	= meme_truncate,
	.utimens	= meme_utimens,
	.open		= meme_open,
	.read		= meme_read,
	.write		= meme_write,
	.statfs		= meme_statfs,
	.release	= meme_release,
	.fsync		= meme_fsync,
#ifdef HAVE_SETXATTR
	.setxattr	= meme_setxattr,
	.getxattr	= meme_getxattr,
	.listxattr	= meme_listxattr,
	.removexattr	= meme_removexattr,
#endif
};

int main(int argc, char *argv[])
{
	current_path = get_current_dir_name();
	umask(0);
	return fuse_main(argc, argv, &meme_oper, NULL);
}
