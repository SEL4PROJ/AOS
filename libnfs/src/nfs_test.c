/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#include <nfs/nfs.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

#include "rpc.h"
#include "common.h"

#define REPS 20

#define NOBODY    65534
#define NOGROUP   65534
#define ERTOS_SRC  1105

#define ACC_MODE   0100764
#define USER       NOBODY
#define GROUP      NOGROUP

#define SKIP(x) do { if(0){ x ;} }while(0)
#define RUN(x)  do { if(1){ x ;} }while(0)

#define FILE1 "file1"

#define ERR_STR(x) (((x))? "FAILED" : "SUCCESS")
#define PRINT_RESULT(err) printf("%-60s> %s\n", __func__, ERR_STR(err))
#define PRINT_WELCOME()   printf("Testing %s\n", __func__)

/* Differentiate between memory leaks in this code and leaks in nfs code */
static int internal_malloc = 0;
static void*
my_malloc(size_t s)
{
    internal_malloc++;
    return malloc(s);
}

static void
my_free(void* a)
{
    internal_malloc--;
    free(a);
}

static void
wait(int *v)
{
    while(!*v){
        _usleep(1000);
        rpc_timeout(1);
    }
}

/* Check for leaks. Only works with libsel4c */
static int mallocs = 0;
static void
heap_test_start(void)
{
#ifdef CONFIG_LIB_SEL4_C
    mallocs = malloc_leak_check();
#endif
}

static int
heap_test_end(void)
{
#ifdef CONFIG_LIB_SEL4_C
    return mallocs - malloc_leak_check();
#else
    return mallocs;
#endif
}

/**************** HELPER ROUTINES ***************/
/************************************************/

static int
check_sfattr(sattr_t *sattr, fattr_t *fattr){
    int err = 0;
    if(sattr->mode != fattr->mode){
        printf("mode mismatch (0%o|0%o)\n", sattr->mode, fattr->mode);
        assert(0);
        err++;
    }
    if(sattr->uid != fattr->uid){
        printf("uid mismatch (%d|%d)\n", sattr->uid, fattr->uid);
        err++;
    }
    if(sattr->gid != fattr->gid){
        printf("gid mismatch (%d|%d)\n", sattr->gid, fattr->gid);
        err++;
    }
    if(sattr->size != fattr->size){
        printf("size mismatch (%d|%d)\n", sattr->size, fattr->size);
        err++;
    }
#if 0 /* No point in checking access time: set by server */
    if(sattr->atime.seconds != fattr->atime.seconds){
        printf("access us mismatch (%d|%d)\n", sattr->atime.seconds, 
                                        fattr->atime.seconds);
        err++;
    }
    if(sattr->atime.useconds != fattr->atime.useconds){
        printf("access us mismatch (%d|%d)\n", sattr->atime.useconds,
                                         fattr->atime.useconds);
        err++;
    }
#endif
    if(sattr->mtime.seconds != fattr->mtime.seconds){
        printf("mod ms mismatch (%d|%d)\n", sattr->mtime.seconds,
                                        fattr->mtime.seconds);
        err++;
    }
#if 0 /* Ignore micro seconds */
    if(sattr->mtime.useconds != fattr->mtime.useconds){
        printf("mod us mismatch (%d|%d)\n", sattr->mtime.useconds,
                                         fattr->mtime.useconds);
        err++;
    }
#endif
    return err;
}

/*************** READ DIR **********************/

struct my_readdir_arg {
    int v;
    char ***files;
    int *nfiles;
    enum nfs_stat stat;
    fhandle_t *pfh;
};

static void
print_files(int nfiles, char **files){
    int i;
    printf("Directory listing:\n");
    for(i = 0; i < nfiles; i++){
        printf("%02d|%s\n", i, *files++);
    }
}

static void 
my_readdir_cb(uintptr_t token, enum nfs_stat status, int nfiles,
              char* file_names[], nfscookie_t nfscookie){
    struct my_readdir_arg * arg = (struct my_readdir_arg*)token;
    int total_files = *arg->nfiles + nfiles;
    if(arg->files){
        char **files = (char**)my_malloc(sizeof(*files)*total_files);
        assert(status == NFS_OK);
        /* copy in the original list by reference */
        memcpy(files, *arg->files, *arg->nfiles * sizeof(char*));
        /* copy in the new list and its entries */
        int i, j;
        for(i = 0, j = *arg->nfiles; i < nfiles; i++, j++){
            files[j] = (char*)my_malloc(strlen(file_names[i]) + 1);
            strcpy(files[j], file_names[i]);
        }
        /* clean up old list - entries were copied by reference */
        if(*arg->files){
            my_free(*arg->files);
        }
        /* update records */
        *arg->files = files;
    }
    *arg->nfiles = total_files;
    if(nfscookie != 0){
        nfs_readdir(arg->pfh, nfscookie, &my_readdir_cb, token);
    }else{
        arg->stat = status;
        arg->v = 1;
    }
}



static int 
my_readdir(fhandle_t *pfh, int *nfiles, char ***files){
    struct my_readdir_arg arg;
    *nfiles = 0;
    if(files){
        *files = NULL;
    }
    arg.v = 0;
    arg.files = files;
    arg.nfiles = nfiles;
    arg.pfh = pfh;
    arg.stat = NFS_OK;
    assert(!nfs_readdir(pfh, 0, &my_readdir_cb, (uintptr_t)&arg));
    wait(&arg.v);
    return arg.stat;
}

static void 
my_readdir_clean(int nfiles, char** files){
    int i;
    for(i = 0; i < nfiles; i++){
        my_free(files[i]);
    }
    if(nfiles){
        my_free(files);
    }
}

/*************** LOOKUP **********************/

struct my_lookup_arg {
    int v;
    fattr_t* fattr;
    fhandle_t *fh;
    enum nfs_stat stat;
};

static void 
my_lookup_cb(uintptr_t token, enum nfs_stat status, fhandle_t* fh, 
             fattr_t* fattr){
    struct my_lookup_arg *arg = (struct my_lookup_arg*)token;
    if(arg->fattr != NULL){
        memcpy(arg->fattr, fattr, sizeof(*fattr));
    }
    if(arg->fh != NULL){
        memcpy(arg->fh, fh, sizeof(*fh));
    }
    arg->stat = status;
    arg->v = 1;
}

static enum nfs_stat
my_lookup(fhandle_t *mnt, char* name, fhandle_t *fh, fattr_t *fattr){
    struct my_lookup_arg arg;
    arg.v = 0;
    arg.fattr = fattr;
    arg.fh = fh;
    arg.stat = 0;
    assert(!nfs_lookup(mnt, name, &my_lookup_cb, (uintptr_t)&arg));
    wait(&arg.v);
    return arg.stat;
}


/*************** CREATE **********************/
struct my_create_arg {
    int v;
    fhandle_t *fh;
    enum nfs_stat stat;
    fattr_t *fattr;
};

static void
my_create_cb(uintptr_t token, enum nfs_stat stat, fhandle_t *fh, fattr_t *fattr){
    struct my_create_arg *arg = (struct my_create_arg*)token;
    (void)fh;
    arg->stat = stat;
    if(arg->fattr){
        memcpy(arg->fattr, fattr, sizeof(*fattr));
    }
    if(arg->fh){
        memcpy(arg->fh, fh, sizeof(*fh));
    }
    arg->v = 1;
}

static enum nfs_stat
my_create(fhandle_t *pfh, char* name, sattr_t* sattr, fattr_t* fattr,
            fhandle_t *fh){
    struct my_create_arg arg;
    arg.v = 0;
    arg.stat = NFS_OK;
    arg.fattr = fattr;
    arg.fh = fh;
    assert(!nfs_create(pfh, name, sattr, &my_create_cb, (uintptr_t)&arg));
    wait(&arg.v);
    return arg.stat;
}

/*************** REMOVE **********************/
struct my_remove_arg {
    int v;
    enum nfs_stat stat;
};

static void
my_remove_cb(uintptr_t token, enum nfs_stat status){
    struct my_remove_arg *arg = (struct my_remove_arg*)token;
    arg->stat = status;
    arg->v = 1;
}

static enum nfs_stat
my_remove(fhandle_t *mnt, char* name){
    struct my_remove_arg arg;
    arg.v = 0;
    arg.stat = NFS_OK;
    assert(!nfs_remove(mnt, name, &my_remove_cb, (uintptr_t)&arg));
    wait(&arg.v);
    return arg.stat;
}

/*************** GETATTR **********************/
struct my_getattr_arg {
    int v;
    enum nfs_stat stat;
    fattr_t *fattr;
};

static void
my_getattr_cb(uintptr_t token, enum nfs_stat status, fattr_t *fattr){
    struct my_getattr_arg *arg = (struct my_getattr_arg*)token;
    arg->stat = status;
    if(arg->fattr){
        memcpy(arg->fattr, fattr, sizeof(*fattr));
    }
    arg->v = 1;
}

static enum nfs_stat
my_getattr(fhandle_t *mnt, char* name, fattr_t *fattr){
    struct my_getattr_arg arg;
    fhandle_t fh;
    assert(my_lookup(mnt, name, &fh, NULL) == NFS_OK);
    arg.v = 0;
    arg.stat = NFS_OK;
    arg.fattr = fattr;
    assert(!nfs_getattr(&fh, &my_getattr_cb, (uintptr_t)&arg));
    wait(&arg.v);
    return arg.stat;
}

/*************** READ **********************/
struct my_read_arg {
    int v;
    fhandle_t *fh;
    char *data;
    int length;
    int offset;
    enum nfs_stat stat;
    int* err;
};

static void
my_read_cb(uintptr_t token, enum nfs_stat stat, fattr_t * fattr, int read, void* _data){
    struct my_read_arg *arg = (struct my_read_arg*)token;
    char *data = (char*)_data;
    (void)fattr;
    /* Stat error */
    if(stat != NFS_OK){
        arg->stat = stat;
        arg->v = 1;
    }else if(read > 0){
        int i;
        for(i = 0; i < read; i++){
            if(arg->data[i] != data[i]){
                printf("Data mismatch on read\n");
                *arg->err = *arg->err + 1;
            }
        }
        arg->data += read;
        arg->offset += read;
        arg->length -= read;
        if(arg->length == 0){
            arg->stat = NFS_OK;
            arg->v = 1;
        }else{
            assert(!nfs_read(arg->fh, arg->offset, arg->length, &my_read_cb, token));
        }
    }else{
        /* didn't read anything??? */
        arg->stat = -1;
        arg->v = 1;
    }
}

static enum nfs_stat
my_read(fhandle_t *fh, int offset, int length, void* data, int* err){
    struct my_read_arg arg = {
            .fh = fh,
            .data = data,
            .length = length,
            .offset = offset,
            .stat = NFS_OK,
            .err = err,
            .v = 0
        };
    assert(!nfs_read(fh, offset, length, &my_read_cb, (uintptr_t)&arg));
    wait(&arg.v);
    return arg.stat;
}


/*************** WRITE **********************/
struct my_write_arg {
    int v;
    fhandle_t *fh;
    char *data;
    int length;
    int offset;
    enum nfs_stat stat;
    int* err;
};

static void
my_write_cb(uintptr_t token, enum nfs_stat status, fattr_t *fattr, int count){
    struct my_write_arg *arg = (struct my_write_arg*)token;
    (void)fattr;
    (void)arg->err;
    if(status != NFS_OK || arg->length <= count){
        arg->stat = status;
        arg->v = 1;
    }else {
        assert(count != 0);
        arg->data += count;
        arg->offset += count;
        arg->length -= count;
        assert(!nfs_write(arg->fh, arg->offset, arg->length, arg->data,
                          &my_write_cb, token));
    }
}

static enum nfs_stat
my_write(fhandle_t *fh, int offset, int length, char* data, int *err){
    struct my_write_arg arg = {
            .fh = fh,
            .data = data,
            .length = length,
            .offset = offset,
            .err = err,
            .v = 0,
            .stat = NFS_OK
        };
    assert(!nfs_write(fh, offset, length, data, &my_write_cb, (uintptr_t)&arg));
    wait(&arg.v);
    return arg.stat;
}

/**************** TEST ROUTINES ***************/
/**********************************************/
#define PARALLEL 2

/* Guarantee to fill multiple packets */
#define TEST_DATA_SIZE (4096 * 2)

static int 
test_file_access(struct fhandle *mnt)
{
    sattr_t sattr;
    fhandle_t fh;
    char* data;
    int i;
    int err = 0;
    int heap_err;
    PRINT_WELCOME();
    heap_test_start();

    /* create some files file */
    sattr.mode = ACC_MODE;
    sattr.uid = USER;
    sattr.gid = GROUP;
    sattr.size = 0;
    sattr.atime.seconds = 12345000;
    sattr.atime.useconds = 6665000;
    sattr.mtime.seconds = 6780000;
    sattr.mtime.seconds = 44430000;
    assert(my_create(mnt, FILE1, &sattr, NULL, &fh) == NFS_OK);

    /* Generate some random* data */
    data = (char*)malloc(TEST_DATA_SIZE * 2);
    for(i = 0; i < TEST_DATA_SIZE; i++){
        data[2 * i + 0] = i >> 8;
        data[2 * i + 1] = i >> 0;
    }
    /* write all data to file 1 at offset 0 */
    assert(my_write(&fh,   0, TEST_DATA_SIZE +   0, data +   0, &err) == NFS_OK);
    /* read all data from file 1 at offset 0 */
    assert( my_read(&fh,   0, TEST_DATA_SIZE +   0, data +   0, &err) == NFS_OK);
    /* read from file 1 at offset 100 */
    assert( my_read(&fh, 100, TEST_DATA_SIZE - 100, data + 100, &err) == NFS_OK);
    /* write all data to file 1 at offset 100 */
    assert(my_write(&fh, 100, TEST_DATA_SIZE +   0, data +   0, &err) == NFS_OK);
    /* read make sure that the data was written correctly to file 1 */
    assert( my_read(&fh,   0,              0 + 100, data +   0, &err) == NFS_OK);
    assert( my_read(&fh, 100, TEST_DATA_SIZE +   0, data +   0, &err) == NFS_OK);
    /* Make sure we do not overwrite data */
    assert(my_write(&fh,   0,              0 + 100, data +   0, &err) == NFS_OK);
    assert( my_read(&fh, 100, TEST_DATA_SIZE +   0, data +   0, &err) == NFS_OK);
    /* Make sure the entire contents of the file is as expected */
    memmove(data + 100, data, TEST_DATA_SIZE);
    assert( my_read(&fh,   0, TEST_DATA_SIZE + 100, data +   0, &err) == NFS_OK);
    /* DONE */

    /* Clean up the file */
    assert( my_remove(mnt, FILE1) == NFS_OK);

    free(data);
    heap_err = heap_test_end();
    err += heap_err;
    PRINT_RESULT(err);
    return err;
}



static int
_create_files(struct fhandle *mnt, char* fname_data, sattr_t *sattr){
    /* Create the files - do this in parallel */
    struct my_create_arg arg[PARALLEL];
    struct my_create_arg argdef;
    int i;
    int err = 0;

    /* Set default call argument */
    argdef.v = 0;
    argdef.stat = NFS_OK;
    argdef.fattr = NULL;
    argdef.fh = NULL;

    i = 1;
    while(i < MAXNAMLEN){
        int k, j;
        for(j = 0, k = i; j < PARALLEL && k < MAXNAMLEN; j++, k++){
            char *fname = &fname_data[MAXNAMLEN - k];
            arg[j] = argdef;
            assert(!nfs_create(mnt, fname, sattr, 
                               &my_create_cb, (uintptr_t)&arg[j]));
        }
        for(j = 0, k = i; j < PARALLEL && k < MAXNAMLEN; j++, k++){
            wait(&arg[j].v);
            if(arg[j].stat != NFS_OK){
                printf("Failed to create file of name size %d\n", k);
                err++;
            }
        }
        i += PARALLEL;
    }
    return err;
}

static int
_check_for_files(struct fhandle *mnt, char* fname_data){
    /* Check that the files are there */
    int nfiles;
    char **remote_fnames = NULL;
    int err = 0;
    int i, j;

    /* Read dir contents */
    if(my_readdir(mnt, &nfiles, &remote_fnames) != NFS_OK){
        printf("readdir failed\n");
        err++;
    } else if(nfiles == MAXNAMLEN + 2 /* . and .. */){
        printf("Odd number of files after file name length test (%d)\n",
                nfiles);
        print_files(nfiles, remote_fnames);
        err++;
    }
    /* Check names */
    for(i = 1; i < MAXNAMLEN; i++){
        char *fname = &fname_data[MAXNAMLEN - i];
        int found = 0;
        for(j = 0; j < nfiles && !found; j++){
            char *f = remote_fnames[j];
            found = strcmp(fname, f) == 0;
        }
        if(!found){
            printf("Unable to find file with name %s\n", fname);
            err++;
        }
    }
    /* clean up */
    my_readdir_clean(nfiles, remote_fnames);
    return err;
}

static int
_remove_files(struct fhandle *mnt, char* fname_data){
    struct my_remove_arg arg[PARALLEL];
    int nfiles;
    int err = 0;
    int i;
    /* Remove the files */
    i = 1;
    while(i < MAXNAMLEN){
        int j, k;
        for(j = 0, k = i; j < PARALLEL && k < MAXNAMLEN; j++, k++){
            char *fname = &fname_data[MAXNAMLEN - k];
            arg[j].v = 0;
            assert(nfs_remove(mnt, fname, &my_remove_cb, 
                              (uintptr_t)&arg[j]) == RPC_OK);
        }
        for(j = 0, k = i; j < PARALLEL && k < MAXNAMLEN; j++, k++){
            wait(&arg[j].v);
            if(arg[j].stat != NFS_OK){
                printf("Failed to create file of name size %d\n", k);
                err++;
            }
        }
        i += PARALLEL;
    }

    /* Check that no files remain */
    if(my_readdir(mnt, &nfiles, NULL) != NFS_OK){
        printf("readdir failed\n");
        err++;
    }else if(nfiles != 2){
        printf("Files left over after file name length test");
        err++;
    }

    return err;
}

static int
test_file_names(struct fhandle *mnt)
{
    struct sattr sattr;
    char fname_data[MAXNAMLEN + 1];
    char *fname;
    int heap_err;
    int err = 0;
    int i,j;
    heap_test_start(); 

    /* Set default file attributes */
    sattr.mode = ACC_MODE;
    sattr.uid = USER;
    sattr.gid = GROUP;
    sattr.size = 0;
    sattr.atime.seconds = 12345000;
    sattr.atime.useconds = 6665000;
    sattr.mtime.seconds = 6780000;
    sattr.mtime.seconds = 44430000;

    /* Generate a file name */
    fname = fname_data;
    for(i = 0; i < (MAXNAMLEN + 1)/16/2; i++){
        for(j = 0; j < 16; j++){
            *fname = 'A' + j;
            fname++;
            *fname = 'a' + i;
            fname++;
        }
    }
    fname_data[MAXNAMLEN] = '\0';

    /* Perform the tests */
    err += _create_files(mnt, fname_data, &sattr);
    err += _check_for_files(mnt, fname_data);
    err += _remove_files(mnt, fname_data);

    heap_err = heap_test_end();
    printf("%s> errors: %d leaks: %d\n", __func__, err, heap_err);
    err += heap_err;
    if(err){
        PRINT_RESULT(err);
    }
    return err;
}

/***********************************************/


static int 
test_file_creation(struct fhandle *mnt)
{
    int heap_err;
    int err = 0;
    int nfiles;
    enum nfs_stat stat;
    sattr_t sattr;
    fattr_t fattr;

    PRINT_WELCOME();

    heap_test_start();
    /* Check lookup fails */
    if((stat = my_lookup(mnt, FILE1, NULL, NULL)) != NFSERR_NOENT){
        printf("lookup found a file (%s) that should not be there"
               "Error %d\n", FILE1, stat);
        assert(0);
        err++;
    }
    if(my_readdir(mnt, &nfiles, NULL) != NFS_OK){
        printf("readdir failed\n");
        err++;
    }
    if(nfiles != 2){
        printf("There are files present. Should be empty\n");
        err++;
    }
    /* check remove file */
    if(my_remove(mnt, FILE1) == NFS_OK){
        printf("Removed a file that didn't exist\n");
        err++;
    }


    /*** create a file ***/
    sattr.mode = ACC_MODE;
    sattr.uid = USER;
    sattr.gid = GROUP;
    sattr.size = 0;
    sattr.atime.seconds = 12345000;
    sattr.atime.useconds = 6665000;
    sattr.mtime.seconds = 6780000;
    sattr.mtime.seconds = 44430000;
    stat = my_create(mnt, FILE1, &sattr, &fattr, NULL);
    if(stat != NFS_OK){
        printf("Error creating file (%d)\n", stat);
        assert(0);
    }
    if(check_sfattr(&sattr, &fattr)){
        printf("New file attributes not set\n");
        err++;
    }
    /*********************/

    /* check number of files */
    if(my_readdir(mnt, &nfiles, NULL) != NFS_OK){
        printf("Read dir failed\n");
        err++;
    }
    if(nfiles != 3){
        printf("The file count is wrong once a file has been created\n");
        err++;
    }
    /* Check lookup succeeds */
    if(my_lookup(mnt, FILE1, NULL, NULL) != NFS_OK){
        printf("lookup could not file the new file\n");
        err++;
    }
#if 0 
 /* This will return NFS_OK. I presume that this test is only valid if we do
  * not have permission to write to the existing file.
  */
    /* Can't create a duplicate file */
    if(my_create(mnt, FILE1, &sattr, &fattr, NULL) != NFSERR_EXIST){
        printf("Should not have been able to create a file that already exists\n");
        err++;
    }
#endif
    /* check that we can get the attributes */
    if(my_getattr(mnt, FILE1, &fattr) != NFS_OK){
        printf("New file attributes are incorrect\n");
        err++;
    }

    /*** remove the file ***/
    if(my_remove(mnt, FILE1) != NFS_OK){
        printf("Failed to remove the file\n");
        err++;
    }
    /***********************/

    /* Check lookup fails */
    if(my_lookup(mnt, FILE1, NULL, NULL) != NFSERR_NOENT){
        printf("lookup found a file that should not be there\n");
        err++;
    }
    /* check number of files */
    assert(!my_readdir(mnt, &nfiles, NULL));
    if(nfiles != 2){
        printf("There are files present after delete. Should be empty\n");
        err++;
    }
    /* check remove file */
    if(my_remove(mnt, FILE1) == NFS_OK){
        printf("Removed a file that we already removed\n");
        err++;
    }

    heap_err = heap_test_end();
    printf("%s> errors: %d heap errors: %d\n", __func__, err, heap_err);
    err += heap_err;
    PRINT_RESULT(err);
    return err;
}


static int 
test_empty(struct fhandle *mnt)
{
    char** files;
    int nfiles;
    int heap_err;
    int err = 0;
    PRINT_WELCOME();
    heap_test_start();
    assert(!my_readdir(mnt, &nfiles, &files));
    if(nfiles != 2 /* . and .. */){
        print_files(nfiles, files);
        err += nfiles - 2;
    }
    my_readdir_clean(nfiles, files);
    heap_err = heap_test_end();
    assert(internal_malloc == 0);
    printf("found %d files. Leaks: %d\n", nfiles, heap_err);
    err += heap_err;
    PRINT_RESULT(err);
    return err;
}

static int
test_mnt(char* mnt)
{
    int i;
    int export_err = 0;
    int mount1_err = 0;
    int mount2_err = 0;
    int export_heap_err = 0;
    int mount_heap1_err = 0;
    int mount_heap2_err = 0;
    struct fhandle mnt_handle;
    int err = 0;
    PRINT_WELCOME();
    /* run the export test */
    heap_test_start();
    for(i = 0; i < REPS; i++){
        if(nfs_print_exports()){
            export_err ++;
        }
    }
    export_heap_err = heap_test_end();
 
    /* run the mount test */
    heap_test_start();
    for(i = 0; i < REPS; i++){
        if(nfs_mount(mnt, &mnt_handle)){
            mount1_err++;
        }
    }
    mount_heap1_err = heap_test_end();

    /* run the mount test on a bad mount*/
    heap_test_start();
    for(i = 0; i < REPS; i++){
        if(!nfs_mount("BOGUS", &mnt_handle)){
            mount2_err++;
        }
    }
    mount_heap2_err = heap_test_end();

    printf("export errors: %d, heap error: %d\n", export_err, export_heap_err);
    printf("mount  errors: %d, heap error: %d\n", mount1_err, mount_heap1_err);
    printf("export errors: %d, heap error: %d\n", mount2_err, mount_heap2_err);
    err += export_err + mount1_err + mount2_err;
    err += export_heap_err + mount_heap1_err + mount_heap2_err;
    PRINT_RESULT(err);
    return err;
}


static void
my_retx_lookup_cb(uintptr_t token, enum nfs_stat status, fhandle_t* fh, fattr_t* fattr)
{
    uint32_t* t = (uint32_t*)token;
    (void)fh;
    (void)fattr;
    (void)status;
    *t = *t + 1;
}

#define RETX_REPEATS  20
#define RETX_TIMEOUTS 20
static int
test_retransmit(struct fhandle* pfh)
{
    int32_t token[RETX_REPEATS];
    int i, j;
    int err = 0;
    int heap_err;
    PRINT_WELCOME();
    heap_test_start();
    /* Send 20 requests */
    for(j = 0; j < RETX_REPEATS; j++){
        token[j] = 0;
        /* perform single lookup with forced retransmitts. Count responses */
        assert(!nfs_lookup(pfh, "gjhg", &my_retx_lookup_cb, (uintptr_t)&token[j]));
        for(i = 0; i < RETX_TIMEOUTS; i++){
            nfs_timeout();
        }
        wait(&token[j]);
    }
    /* Make sure we only got 1 response per request */
    for(j = 0; j < RETX_REPEATS; j++){
        if(token[j] != 1){
            err++;
        }
    }
    /* Epilogue */
    heap_err = heap_test_end();
    printf("%s> errors: %d leaks: %d\n", __func__, err, heap_err);
    err += heap_err;
    PRINT_RESULT(err);
    return err;
}

int 
nfs_test(char *mnt)
{
    struct fhandle mnt_handle;
    int err = 0;
    printf("*****************\n");
    printf("*** NFS TESTS ***\n");
    printf("*****************\n");

    /* Test mountd */
    RUN(err += test_mnt(mnt));
    /* Open the mount point and check that it is clean */
    if(nfs_mount(mnt, &mnt_handle)){
        printf("*** Unable to mount %s\n", mnt);
        assert(0);
        return -1;
    }
    if(test_empty(&mnt_handle)){
        printf("*** Mount dir not empty!\n");
        assert(0);
        return -1;
    }
    /* Check file creation removal and attributes */
    RUN(err += test_file_creation(&mnt_handle));
    /* Check various file name length and combos */
    RUN(err += test_file_names(&mnt_handle));
    /* Check file read/write access */
    RUN(err += test_file_access(&mnt_handle));
    /* Check that retransmittions succeed */
    RUN(err += test_retransmit(&mnt_handle));

    printf("NFS tests found %d errors: \t\t\t %s\n", err, ERR_STR(err));
    return err;
}

