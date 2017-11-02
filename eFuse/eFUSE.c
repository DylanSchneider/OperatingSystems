/*
  FUSE: Filesystem in Userspace
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>

  Minor modifications and note by Andy Sayler (2012) <www.andysayler.com>

  Source: fuse-2.8.7.tar.gz examples directory
  http://sourceforge.net/projects/fuse/files/fuse-2.X/

  This program can be distributed under the terms of the GNU GPL.
  See the file COPYING.

  gcc -Wall `pkg-config fuse --cflags` fusexmp.c -o fusexmp `pkg-config fuse --libs`

  Note: This implementation is largely stateless and does not maintain
        open file handels between open and release calls (fi->fh).
        Instead, files are opened and closed as necessary inside read(), write(),
        etc calls. As such, the functions that rely on maintaining file handles are
        not implmented (fgetattr(), etc). Those seeking a more efficient and
        more complete implementation may wish to add fi->fh support to minimize
        open() and close() calls and support fh dependent functions.

*/

#define FUSE_USE_VERSION 28

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef linux
/* For pread()/pwrite() */
#define _XOPEN_SOURCE 500
#endif

#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include "aes-crypt.h"	//1 encrypts, 0 decrypts, -1 copies
#include <dirent.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/xattr.h>
#include <linux/limits.h>

#define eFUSE_data ((struct eFUSE_state *) fuse_get_context()->private_data)
#define USAGE "./eFUSE <Encryption Password> <Mirror Root Directory> <Mount Target Directory>\n"

#define ENCRYPT 1
#define DECRYPT 0
#define COPY -1

struct eFUSE_state{
    char* mirror_path;
    char* crypt_password;
};

static void full_path(const char* path, char nPath[PATH_MAX])
{
    strcpy(nPath, eFUSE_data->mirror_path);
    strncat(nPath, path, PATH_MAX);
}

static int eFUSE_getattr(const char *path, struct stat *stbuf)
{
	int res;
    char new_path[PATH_MAX];
    full_path(path, new_path);
	res = lstat(new_path, stbuf);
	if (res == -1)
		return -errno;
    
	return 0;
}

static int eFUSE_access(const char *path, int mask)
{
	int res;
    char new_path[PATH_MAX];
    full_path(path, new_path);
	res = access(new_path, mask);
	if (res == -1)
		return -errno;

	return 0;
}

static int eFUSE_readlink(const char *path, char *buf, size_t size)
{
	int res;
    char new_path[PATH_MAX];
    full_path(path, new_path);
	res = readlink(new_path, buf, size - 1);
	if (res == -1)
		return -errno;

	buf[res] = '\0';
	return 0;
}


static int eFUSE_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
		       off_t offset, struct fuse_file_info *fi)
{
	DIR *dp;
	struct dirent *de;
    
    char new_path[PATH_MAX];
    full_path(path, new_path);
    
	(void) offset;
	(void) fi;

	dp = opendir(new_path);
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

static int eFUSE_mknod(const char *path, mode_t mode, dev_t rdev)
{
	int res;
    char new_path[PATH_MAX];
    full_path(path, new_path);
    
	/* On Linux this could just be 'mknod(path, mode, rdev)' but this
	   is more portable */
	if (S_ISREG(mode)) {
		res = open(new_path, O_CREAT | O_EXCL | O_WRONLY, mode);
		if (res >= 0)
			res = close(res);
	} else if (S_ISFIFO(mode))
		res = mkfifo(new_path, mode);
	else
		res = mknod(new_path, mode, rdev);
	if (res == -1)
		return -errno;

	return 0;
}

static int eFUSE_mkdir(const char *path, mode_t mode)
{
	int res;
    char new_path[PATH_MAX];
    full_path(path, new_path);
	res = mkdir(new_path, mode);
	if (res == -1)
		return -errno;

	return 0;
}

static int eFUSE_unlink(const char *path)
{
	int res;
    char new_path[PATH_MAX];
    full_path(path, new_path);
	res = unlink(new_path);
	if (res == -1)
		return -errno;

	return 0;
}

static int eFUSE_rmdir(const char *path)
{
	int res;
    char new_path[PATH_MAX];
    full_path(path, new_path);
	res = rmdir(new_path);
	if (res == -1)
		return -errno;

	return 0;
}

static int eFUSE_symlink(const char *from, const char *to)
{
	int res;
    char new_tpath[PATH_MAX];
    full_path(to, new_tpath);
	res = symlink(from, new_tpath);
	if (res == -1)
		return -errno;

	return 0;
}

static int eFUSE_rename(const char *from, const char *to)
{
	int res;
    char new_fpath[PATH_MAX];
    char new_tpath[PATH_MAX];
    full_path(from, new_fpath);
    full_path(to, new_tpath);
	res = rename(new_fpath, new_tpath);
	if (res == -1)
		return -errno;

	return 0;
}

static int eFUSE_link(const char *from, const char *to)
{
	int res;
    char new_fpath[PATH_MAX];
    char new_tpath[PATH_MAX];
    full_path(from, new_fpath);
    full_path(to, new_tpath);
	res = link(new_fpath, new_tpath);
	if (res == -1)
		return -errno;

	return 0;
}

static int eFUSE_chmod(const char *path, mode_t mode)
{
	int res;
    char new_path[PATH_MAX];
    full_path(path, new_path);
	res = chmod(new_path, mode);
	if (res == -1)
		return -errno;

	return 0;
}

static int eFUSE_chown(const char *path, uid_t uid, gid_t gid)
{
	int res;
    char new_path[PATH_MAX];
    full_path(path, new_path);
	res = lchown(new_path, uid, gid);
	if (res == -1)
		return -errno;

	return 0;
}

static int eFUSE_truncate(const char *path, off_t size)
{
	int res;
    char new_path[PATH_MAX];
    full_path(path, new_path);
	res = truncate(new_path, size);
	if (res == -1)
		return -errno;

	return 0;
}

static int eFUSE_utimens(const char *path, const struct timespec ts[2])
{
	int res;
	struct timeval tv[2];

	tv[0].tv_sec = ts[0].tv_sec;
	tv[0].tv_usec = ts[0].tv_nsec / 1000;
	tv[1].tv_sec = ts[1].tv_sec;
	tv[1].tv_usec = ts[1].tv_nsec / 1000;

    char new_path[PATH_MAX];
    full_path(path, new_path);
	res = utimes(new_path, tv);
	if (res == -1)
		return -errno;

	return 0;
}

static int eFUSE_open(const char *path, struct fuse_file_info *fi)
{
	int res;
    char new_path[PATH_MAX];
    full_path(path, new_path);
	res = open(new_path, fi->flags);
	if (res == -1)
		return -errno;

	close(res);
	return 0;
}

static int eFUSE_read(const char *path, char *buf, size_t size, off_t offset,
		    struct fuse_file_info *fi)
{
    int res;
    char new_path[PATH_MAX];
    full_path(path, new_path);
    
    (void) fi;
    (void) offset;
    
    //get xattr_value
    FILE *currentFile = fopen(new_path, "r");
    FILE *temp = tmpfile();
    
    if(currentFile == NULL)
        return -errno;
    
    ssize_t xattr_length = getxattr(new_path, "user.encrypted", NULL, 0);
    char* xattr_value = malloc(sizeof(*xattr_value) * (xattr_length + 1));
    xattr_length = getxattr(new_path, "user.encrypted", xattr_value, xattr_length);
    xattr_value[xattr_length] = '\0';
    
    if (!strcmp(xattr_value, "true")) //if encrypted
    {
        do_crypt(currentFile, temp, DECRYPT, eFUSE_data -> crypt_password); //decrypt
        
        // reset pointers
        rewind(currentFile);
        rewind(temp);
        
        // read decrypted file
        res = fread(buf, 1, size, temp);
        if (res == -1)
            res = -errno;
    }
    else //not encrypted, normal read
    {
        res = fread(buf, 1, size, currentFile);
        if (res == -1)
            res = -errno;
    }
    
    free(xattr_value);
    
    // close files
    fclose(currentFile);
    fclose(temp);
    
    return res;
}

static int eFUSE_write(const char *path, const char *buf, size_t size,
		     off_t offset, struct fuse_file_info *fi)
{
    int res;
    char new_path[PATH_MAX];
    full_path(path, new_path);
    
    (void) fi;
    
    //get xattr_value
    FILE *currentFile = fopen(new_path, "r+");
    FILE *temp = tmpfile();
    
    if(currentFile == NULL)
        return -errno;
    
    
    ssize_t xattr_length = getxattr(new_path, "user.encrypted", NULL, 0);
    char* xattr_value = malloc(sizeof(*xattr_value) * (xattr_length + 1));
    xattr_length = getxattr(new_path, "user.encrypted", xattr_value, xattr_length);
    xattr_value[xattr_length] = '\0';
    
    
    if (!strcmp(xattr_value, "true")) // If encrypted
    {
        do_crypt(currentFile, temp, DECRYPT, eFUSE_data -> crypt_password); // decrypt
    }
    else
    {
        do_crypt(currentFile, temp, COPY, eFUSE_data -> crypt_password); // not encrypted, just make a copy
    }
    
    // always write to temp
    res = pwrite(fileno(temp), buf, size, offset);
    if (res == -1)
        res = -errno;
    
    // reset file pointers
    rewind(currentFile);
    rewind(temp);
    
    // encrypt on close if file was encrypted before
    if (!strcmp(xattr_value, "true"))
    {
        do_crypt(temp, currentFile, ENCRYPT, eFUSE_data -> crypt_password);
    }
    else
    {
        do_crypt(temp, currentFile, COPY, eFUSE_data -> crypt_password);
    }
    
    // reset file pointers
    rewind(currentFile);
    rewind(temp);
    
    free(xattr_value);
    
    // close files
    fclose(currentFile);
    fclose(temp);
    
    return res;
}

static int eFUSE_statfs(const char *path, struct statvfs *stbuf)
{
	int res;
    char new_path[PATH_MAX];
    full_path(path, new_path);
	res = statvfs(new_path, stbuf);
	if (res == -1)
		return -errno;

	return 0;
}

static int eFUSE_create(const char* path, mode_t mode, struct fuse_file_info* fi) {

    (void) fi;

    int res;
    char new_path[PATH_MAX];
    full_path(path, new_path);
    res = creat(new_path, mode);
    if(res == -1)
        return -errno;
    
    //add encrypted attribute to files created
    setxattr(new_path, "user.encrypted", "true", 4, 0);

    close(res);

    return 0;
}


static int eFUSE_release(const char *path, struct fuse_file_info *fi)
{
	/* Just a stub.	 This method is optional and can safely be left
	   unimplemented */

	(void) path;
	(void) fi;
	return 0;
}

static int eFUSE_fsync(const char *path, int isdatasync,
		     struct fuse_file_info *fi)
{
	/* Just a stub.	 This method is optional and can safely be left
	   unimplemented */

	(void) path;
	(void) isdatasync;
	(void) fi;
	return 0;
}

static int eFUSE_setxattr(const char *path, const char *name, const char *value, size_t size, int flags)
{
    
    int res;
    char new_path[PATH_MAX];
    full_path(path, new_path);
    res = lsetxattr(new_path, name, value, size, flags);
	if (res == -1)
		return -errno;
	return 0;
}

static int eFUSE_getxattr(const char *path, const char *name, char *value,
			size_t size)
{
    int res;
    char new_path[PATH_MAX];
    full_path(path, new_path);
    res = lgetxattr(new_path, name, value, size);
	if (res == -1)
		return -errno;
	return res;
}

static int eFUSE_listxattr(const char *path, char *list, size_t size)
{
    int res;
    char new_path[PATH_MAX];
    full_path(path, new_path);
    res = llistxattr(new_path, list, size);
	if (res == -1)
		return -errno;
	return res;
}

static int eFUSE_removexattr(const char *path, const char *name)
{
    int res;
    char new_path[PATH_MAX];
    full_path(path, new_path);
    res = lremovexattr(new_path, name);
	if (res == -1)
		return -errno;
	return 0;
}

static struct fuse_operations eFUSE_oper = {
	.getattr	= eFUSE_getattr,
	.access		= eFUSE_access,
	.readlink	= eFUSE_readlink,
	.readdir	= eFUSE_readdir,
	.mknod		= eFUSE_mknod,
	.mkdir		= eFUSE_mkdir,
	.symlink	= eFUSE_symlink,
	.unlink		= eFUSE_unlink,
	.rmdir		= eFUSE_rmdir,
	.rename		= eFUSE_rename,
	.link		= eFUSE_link,
	.chmod		= eFUSE_chmod,
	.chown		= eFUSE_chown,
	.truncate	= eFUSE_truncate,
	.utimens	= eFUSE_utimens,
	.open		= eFUSE_open,
	.read		= eFUSE_read,
	.write		= eFUSE_write,
	.statfs		= eFUSE_statfs,
	.create     = eFUSE_create,
	.release	= eFUSE_release,
	.fsync		= eFUSE_fsync,
	.setxattr	= eFUSE_setxattr,
	.getxattr	= eFUSE_getxattr,
	.listxattr	= eFUSE_listxattr,
	.removexattr	= eFUSE_removexattr,
};

int main(int argc, char *argv[])
{
    umask(0);
    
    if(argc != 4){
        printf(USAGE);
        return 1;
    }
    
    struct eFUSE_state* temp_data;
    temp_data = malloc(sizeof(struct eFUSE_state));
    
    if(temp_data == NULL){
        fprintf(stderr, "Memory allocation error\n");
        return 1;
    }
    
    temp_data->crypt_password = argv[1];
    temp_data->mirror_path = realpath(argv[2], NULL);

    
    printf("cypt password -> %s\n", temp_data->crypt_password);
    printf("mirrored path -> %s\n", temp_data->mirror_path);
    
    argv[1] = argv[3];
    argv[2] = NULL;
    argv[3] = NULL;
    
    argc -= 2;
    
    int fuse_ret = fuse_main(argc, argv, &eFUSE_oper, temp_data);
    free(temp_data);
    
    return fuse_ret;
}
