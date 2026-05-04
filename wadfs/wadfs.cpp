#define FUSE_USE_VERSION 26
#define _FILE_OFFSET_BITS 64

#include <fuse.h>
#include <iostream>
#include <string>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <errno.h>
#include "../libWad/Wad.h"

// ANT MESSAGE!: code inspired + used from spec, discussion slides, nmsu + maastaar fuse tutorials - ant :3

using namespace std;


// ------------------------------------------------------------------------------------------------------- FUSE CALLBACKS -------------------------------------------------------------------------------------------

// get attributes for elements from a path
// got from nmsu link bb_getattr
static int wadfs_getattr(const char *path, struct stat *st){
    Wad* wad = (Wad*)fuse_get_context()->private_data;

    memset(st, 0, sizeof(struct stat));

    if (wad->isDirectory(path)){
        st->st_mode = S_IFDIR | 0777; // full perms per spec
        st->st_nlink = 2;
        return 0;
    }

    if (wad->isContent(path)){
        st->st_mode = S_IFREG | 0777;
        st->st_nlink = 1;
        st->st_size = wad->getSize(path);
        return 0;
    }

    return -ENOENT; // not a real path
}

// list directory contents
// got from nmsu link bb_readdir
static int wadfs_readdir(const char *path, void *buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi){
    Wad* wad = (Wad*)fuse_get_context()->private_data;

    if (!wad->isDirectory(path)){
        return -ENOENT;
    }

    // current and parent dir entries
    filler(buffer, ".", NULL, 0);
    filler(buffer, "..", NULL, 0);

    vector<string> entries;
    wad->getDirectory(path, &entries);
    for (int i = 0; i < entries.size(); i++){
        filler(buffer, entries[i].c_str(), NULL, 0);
    }

    return 0;
}

// read file contents
// got from nmsu link bb_read
static int wadfs_read(const char *path, char *buffer, size_t size, off_t offset, struct fuse_file_info *fi){
    Wad* wad = (Wad*)fuse_get_context()->private_data;

    int bytesRead = wad->getContents(path, buffer, (int)size, (int)offset);
    if (bytesRead < 0){
        return -ENOENT;
    }
    return bytesRead;
}

// make directory
// got from maastaar link do_mkdir
static int wadfs_mkdir(const char *path, mode_t mode){
    Wad* wad = (Wad*)fuse_get_context()->private_data;
    wad->createDirectory(path);
    return 0;
}

// make node = create regular file
// got from maastaar link do_mknod
static int wadfs_mknod(const char *path, mode_t mode, dev_t rdev){
    Wad* wad = (Wad*)fuse_get_context()->private_data;
    wad->createFile(path);
    return 0;
}

// write to file
// got from maastaar link do_write
static int wadfs_write(const char *path, const char *buffer, size_t size, off_t offset, struct fuse_file_info *fi){
    Wad* wad = (Wad*)fuse_get_context()->private_data;

    int bytesWritten = wad->writeToFile(path, buffer, (int)size, (int)offset);
    if (bytesWritten < 0){
        return -ENOENT;
    }
    return bytesWritten;
}


// -----------------------------------------------------------------------------------------  FUSE OPERATIONS STRUCT -------------------------------------------------------------------------------------------

static struct fuse_operations wadfs_oper = {
    .getattr = wadfs_getattr,
    .mknod   = wadfs_mknod,
    .mkdir   = wadfs_mkdir,
    .read    = wadfs_read,
    .write   = wadfs_write,
    .readdir = wadfs_readdir,
};


// -------------------------------------------------------------------------------------------------  MAIN --------------------------------------------------------------------------------------------------------------

// usage: ./wadfs -s somewadfile.wad /some/mount/directory
// ta reference example
int main(int argc, char *argv[]){
    if (argc < 3){
        cout << "Usage: ./wadfs -s <wadfile> <mountdir>" << endl;
        return 1;
    }

    // wad path is second to last arg, mount dir is last
    string wadPath = argv[argc - 2];

    // convert relative path to avoid "weird paths"
    if (wadPath[0] != '/'){
        char *cwd = get_current_dir_name();
        wadPath = string(cwd) + "/" + wadPath;
        free(cwd);
    }

    // load wad object
    Wad* wad = Wad::loadWad(wadPath);

    // shuffle argv so fuse doesnt see the wad file arg
    argv[argc - 2] = argv[argc - 1];
    argc--;

    // pass wad pointer as private_data so callbacks can grab it
    return fuse_main(argc, argv, &wadfs_oper, wad);
}