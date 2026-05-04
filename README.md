**GitHub Description:**
```
A FUSE-based userspace filesystem that mounts DOOM WAD files as browsable Linux directories, built in C++ for COP4600 (Operating Systems) at the University of Florida.
```

---

**README.md:**

```markdown
# Custom Linux File System
A FUSE-based userspace filesystem that parses and mounts DOOM WAD files as browsable Linux directory trees. Developed as part of COP4600 (Operating Systems) at the University of Florida.

## Disclaimer
These projects are mine and mine alone. If any form of plagiarism is enacted based on my original work, I will not be afraid to take action in order to defend my self-image and personal integrity.

---

## Overview
This project implements a userspace filesystem daemon using FUSE that exposes a WAD file (the archive format used by DOOM) as a mountable Linux filesystem. The project is split into two deliverables: a static C++ library (`libWad`) that handles all WAD parsing and manipulation, and a FUSE daemon (`wadfs`) that bridges the library to the Linux VFS layer.

The library parses the WAD file's flat descriptor list into an N-ary tree of nodes representing the directory hierarchy, alongside an unordered map for O(1) path lookups. It supports full read and write operations including creating directories, creating files, and writing lump data — all reflected persistently to disk. The FUSE daemon exposes six callbacks (`getattr`, `readdir`, `read`, `mknod`, `mkdir`, `write`) that delegate directly to the library, allowing standard Linux tools like `ls`, `cat`, `echo`, and `mkdir` to interact with WAD file contents as if they were a real filesystem.

## Structure
```
libWad/       # Static library — WAD parsing, tree, read/write ops
  Wad.h
  Wad.cpp
  Makefile

wadfs/        # FUSE daemon — mounts the WAD as a Linux filesystem
  wadfs.cpp
  Makefile
```

## Build
Build the library first, then the daemon:
```bash
cd libWad && make
cd ../wadfs && make
```

To clean build artifacts:
```bash
make clean -C libWad
make clean -C wadfs
```

## Usage
```bash
./wadfs/wadfs -s <wadfile> <mountdir>
```

To unmount:
```bash
fusermount -u <mountdir>
```

## Testing
The library ships with a Google Test suite. Extract it one level above `libWad/` and run:
```bash
sudo chmod +x ./run_libtest.sh
sudo ./run_libtest.sh
```
Target: 35/35 tests passing.

Manual FUSE mount testing:
```bash
mkdir mountdir
./wadfs/wadfs -s -f sample.wad ./mountdir

# in another terminal
ls ./mountdir
cat ./mountdir/mp.txt
echo "hello" > ./mountdir/hi.txt
mkdir ./mountdir/NS
```

## Tools & Technologies
- **Language:** C++17
- **Filesystem:** FUSE (Filesystem in Userspace)
- **File I/O:** POSIX `pread`, `pwrite`, `open`, `close`, `fstat`
- **Data Structures:** N-ary tree + unordered_map
- **Build:** GNU Make, `ar` (static library)
- **Testing:** Google Test
```
