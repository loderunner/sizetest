/*
 FUSE: Filesystem in Userspace
 Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>
 
 This program can be distributed under the terms of the GNU GPL.
 See the file COPYING.
 
 */

/*
 * sizetest OSXFUSE file system in C. Uses the high-level FUSE API.
 * Based on the fusexmp_fh.c example from the Linux FUSE distribution.
 * Amit Singh <http://osxbook.com>
 */

#include <AvailabilityMacros.h>

#if !defined(AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER)
#error "This file system requires Leopard and above."
#endif

#define FUSE_USE_VERSION 28

#define _GNU_SOURCE

#include <fuse.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/xattr.h>
#include <sys/attr.h>
#include <sys/param.h>
#include <sys/vnode.h>

#include "base64.h"

#if defined(_POSIX_C_SOURCE)
typedef unsigned char  u_char;
typedef unsigned short u_short;
typedef unsigned int   u_int;
typedef unsigned long  u_long;
#endif

static struct sizetest_context {
    unsigned long long number_of_files;
    unsigned long long number_of_dirs;
    unsigned long long file_size;
    unsigned long long depth;
} context;

static int fd_counter = 0;

typedef struct _file {
    char* name;
    ino_t ino;
} file_t;

typedef struct _dir {
    char* name;
    ino_t ino;
    struct _file* files;
    unsigned long long number_of_files;
    struct _dir* dirs;
    unsigned long long number_of_dirs;
} dir_t;

static dir_t filesystem = {
    .name = "/",
    .ino = 1,
    .files = NULL,
    .number_of_files = 0,
    .dirs = NULL,
    .number_of_dirs = 0
};

static int recursive_getattr(const char* path, struct stat* stbuf, dir_t* cur_dir)
{
    if (*path == '\0')
    {
        struct fuse_context * fuse_ctx = fuse_get_context();
        nlink_t number_of_entries = cur_dir->number_of_dirs + cur_dir->number_of_files + 2;
        stbuf->st_mode = S_IFDIR | S_IRUSR | S_IXUSR;
        stbuf->st_nlink = number_of_entries;
        stbuf->st_uid = fuse_ctx->uid;
        stbuf->st_gid = fuse_ctx->gid;
        stbuf->st_size = number_of_entries * sizeof(file_t);
        
        return 0;
    }
    
    unsigned long long counter = 0;
    unsigned long maxlen = Base64encode_len(sizeof(counter));
    char name[maxlen];
    char* c = name;
    
    do
    {
        *(c++) = *(path++);
    } while (*path != '/' && *path != '\0');
    
    *c = '\0';
    
    unsigned long len = Base64decode((char*)&counter, name);
    
    if (len == sizeof(counter))
    {
    
    
//    for (unsigned long long i = 0; i < cur_dir->number_of_dirs; i++)
//    {
//        dir_t* d = cur_dir->dirs + i;
//        if (strncmp(d->name, path, strlen(d->name)) == 0)
        if (counter < context.number_of_dirs)
        {
//            path += strlen(d->name);
            if (*path == '\0')
            {
                return recursive_getattr(path, stbuf, NULL);
            }
            else if (*path == '/')
            {
                return recursive_getattr(path+1, stbuf, NULL);
            }
        }
//    }
    
//    for (unsigned long long i = 0; i < cur_dir->number_of_files; i++)
//    {
//        file_t* f = cur_dir->files + i;
//        if (strncmp(path, f->name, strlen(f->name)) == 0)
        if (counter < context.number_of_files + context.number_of_dirs)
        {
            struct fuse_context * fuse_ctx = fuse_get_context();
            
            stbuf->st_mode = S_IFREG | S_IRUSR;
//            stbuf->st_ino = f->ino;
            stbuf->st_nlink = 1;
            stbuf->st_uid = fuse_ctx->uid;
            stbuf->st_gid = fuse_ctx->gid;
            stbuf->st_size = context.file_size;
            
            return 0;
        }
//    }
    }
    
    return -ENOENT;
}

static int
sizetest_getattr(const char *path, struct stat *stbuf)
{
    return recursive_getattr(path+1, stbuf, &filesystem);
}

static dir_t* find_dir(const char* path, dir_t* cur_dir)
{
    if (*path == '\0')
    {
        return cur_dir;
    }
    
    for (unsigned long long i = 0; i < cur_dir->number_of_dirs; i++)
    {
        dir_t* d = cur_dir->dirs + i;
        if (strncmp(d->name, path, strlen(d->name)) == 0)
        {
            path += strlen(d->name);
            if (*path == '\0')
            {
                return find_dir(path, d);
            }
            else if (*path == '/')
            {
                return find_dir(path+1, d);
            }
        }
    }
    
    return NULL;
}



static int
sizetest_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                 off_t offset, struct fuse_file_info *fi)
{
//    dir_t* cur_dir = find_dir(path + 1, &filesystem);
    
//    if (cur_dir != NULL)
    {
        unsigned long long counter = 0;
        unsigned long maxlen = Base64encode_len(sizeof(counter));
        char name[maxlen];
        
        for (unsigned long long i = 0; i < context.number_of_dirs; i++)
        {
            //            dir_t* d = cur_dir->dirs + i;
            Base64encode(name, (char*)&counter, sizeof(counter));
            filler(buf, name, NULL, 0);
            counter++;
        }
        
        for (unsigned long long i = 0; i < context.number_of_files; i++)
        {
//            file_t* f = cur_dir->files + i;
            Base64encode(name, (char*)&counter, sizeof(counter));
            filler(buf, name, NULL, 0);
            counter++;
        }
        
        return 0;
    }
    
    return -ENOENT;
}

static int
sizetest_open(const char *path, struct fuse_file_info *fi)
{
    if (fd_counter != INT_MAX)
    {
        fi->fh = fd_counter++;
        return 0;
    }
    else
    {
        return -1;
    }
}

static const uint8_t deadbeef[4] = {0xde, 0xad, 0xbe, 0xef};

static int
sizetest_read(const char *path, char *buf, size_t size, off_t offset,
              struct fuse_file_info *fi)
{
    const size_t orig_size = size;
    
    while (size > sizeof(deadbeef))
    {
        memcpy(buf, &deadbeef, sizeof(deadbeef));
        buf += sizeof(deadbeef);
        size -= sizeof(deadbeef);
    }
    
    memcpy(buf, &deadbeef, size);
    
    return orig_size;
}

static int
sizetest_statfs(const char *path, struct statvfs *stbuf)
{
    int res;
    
    memset(stbuf, 0, sizeof(stbuf));
    
    stbuf->f_namemax = 255;
    stbuf->f_frsize = 1024;
    stbuf->f_bsize = 1024;
    stbuf->f_flag = ST_RDONLY;
    
    return 0;
}

static int
sizetest_release(const char *path, struct fuse_file_info *fi)
{
    return 0;
}

ino_t recursive_init(dir_t* cur_dir, ino_t cur_ino, unsigned long long cur_depth)
{
    if (cur_depth < context.depth)
    {
        cur_dir->number_of_files = context.number_of_files;
        cur_dir->files = malloc(context.number_of_files * sizeof(file_t));
        for (unsigned long long i = 0; i < context.number_of_files; i++)
        {
            char* random_name = malloc(36);
            snprintf(random_name, 36, "%lx-%lx-%lx-%lx", random(), random(), random(), random());
            
            file_t* f = cur_dir->files + i;
            
            f->name = random_name;
            f->ino = cur_ino++;
        }
        
        cur_dir->number_of_dirs = context.number_of_dirs;
        cur_dir->dirs = malloc(context.number_of_dirs * sizeof(dir_t));
        for (unsigned long long i = 0; i < context.number_of_dirs; i++)
        {
            char* random_name = malloc(36);
            snprintf(random_name, 36, "%lx-%lx-%lx-%lx", random(), random(), random(), random());
            
            dir_t* d = cur_dir->dirs + i;
            
            d->name = random_name;
            d->ino = cur_ino++;
            
            cur_ino = recursive_init(d, cur_ino, cur_depth + 1);
        }
    }
    
    return cur_ino;
}

void *
sizetest_init(struct fuse_conn_info *conn)
{
    FUSE_ENABLE_SETVOLNAME(conn);
    FUSE_ENABLE_XTIMES(conn);
    
    srandomdev();
    
    //recursive_init(&filesystem, 2, 0);
    
    return NULL;
}

void
sizetest_destroy(void *userdata)
{
    /* nothing */
}

static struct fuse_operations sizetest_oper = {
    .init        = sizetest_init,
    .destroy     = sizetest_destroy,
    .getattr     = sizetest_getattr,
    .readdir     = sizetest_readdir,
    .open        = sizetest_open,
    .read        = sizetest_read,
    .statfs      = sizetest_statfs,
    .release     = sizetest_release
};

static const struct fuse_opt sizetest_opts[] = {
    { "--number-of-files=%llu", offsetof(struct sizetest_context, number_of_files), 1 },
    { "--number-of-dirs=%llu", offsetof(struct sizetest_context, number_of_dirs), 0 },
    { "--file-size=%llu", offsetof(struct sizetest_context, file_size), 1024 },
    { "--depth=%llu", offsetof(struct sizetest_context, depth), 1 },
    FUSE_OPT_END
};

/*
 * File/dir count and file size test
 *
 * example usage:
 * 
 * sizetest -f -s -d -o volname=totofuse -o fsname=totofuse -o local --number-of-files=64 --file-size=64 --number-of-dirs=3 --depth=3 /Volumes/totofuse
 *
 * number-of-files: number of regular files per directory
 * file-size: size of files
 * number-of-dirs: number of subdirectories per directory
 * depth: max directory depth
 *
 */

int
main(int argc, char *argv[])
{
    int res = 0;
    struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
    
    if (fuse_opt_parse(&args, &context, sizetest_opts, NULL) == -1) {
		exit(1);
    }
    
    umask(0);
    res = fuse_main(args.argc, args.argv, &sizetest_oper, NULL);
    
    fuse_opt_free_args(&args);
    return res;
}
