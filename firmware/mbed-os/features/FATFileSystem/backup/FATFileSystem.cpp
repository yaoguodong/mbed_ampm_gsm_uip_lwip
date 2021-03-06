/* mbed Microcontroller Library
 * Copyright (c) 2006-2012 ARM Limited
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include "mbed.h"
#include "fatfs.h"
#include "ffconf.h"
#include "mbed_debug.h"

#include "FATFileSystem.h"
#include "FATFileHandle.h"
#include "FATDirHandle.h"

DWORD get_fattime(void) {
    time_t rawtime;
    time(&rawtime);
    struct tm *ptm = localtime(&rawtime);
    return (DWORD)(ptm->tm_year - 80) << 25
         | (DWORD)(ptm->tm_mon + 1  ) << 21
         | (DWORD)(ptm->tm_mday     ) << 16
         | (DWORD)(ptm->tm_hour     ) << 11
         | (DWORD)(ptm->tm_min      ) << 5
         | (DWORD)(ptm->tm_sec/2    );
}

FATFileSystem *FATFileSystem::_ffs[_VOLUMES] = {0};

FATFileSystem::FATFileSystem(const char* n) : FileSystemLike(n) {
		char path[16];
		MX_FATFS_Init();
    debug_if(FFS_DBG, "FATFileSystem(%s)\n", n);
    for(int i=0; i<_VOLUMES; i++) {
        if(_ffs[i] == 0) {
            _ffs[i] = this;
            _fsid = i;
            debug_if(FFS_DBG, "Mounting on ffs drive [%d]\n", _fsid);
						sprintf(path,"%d:/",i);
						f_mount(&_fs, path, 0);
            return;
        }
    }
    error("Couldn't create %s in FATFileSystem::FATFileSystem\n", n);
}

FATFileSystem::~FATFileSystem() {
	char path[16];
	for (int i=0; i<_VOLUMES; i++) {
			if (_ffs[i] == this) {
					_ffs[i] = 0;
					sprintf(path,"%d:/",i);
					f_mount(NULL, path, 1);
			}
	}
}

FileHandle *FATFileSystem::open(const char* name, int flags) {
    debug_if(FFS_DBG, "open(%s) on filesystem, drv [%d]\n", name, _fsid);
    char n[64];
    sprintf(n, "%d:/%s", _fsid, name);
    
    /* POSIX flags -> FatFS open mode */
    BYTE openmode;
    if (flags & O_RDWR) {
        openmode = FA_READ|FA_WRITE;
    } else if(flags & O_WRONLY) {
        openmode = FA_WRITE;
    } else {
        openmode = FA_READ;
    }
    if(flags & O_CREAT) {
        if(flags & O_TRUNC) {
            openmode |= FA_CREATE_ALWAYS;
        } else {
            openmode |= FA_OPEN_ALWAYS;
        }
    }
    
    FAT_FIL fh;
    FRESULT res = f_open(&fh, n, openmode);
    if (res) { 
        debug_if(FFS_DBG, "f_open('w') failed: %d\n", res);
        return NULL;
    }
    if (flags & O_APPEND) {
        f_lseek(&fh, fh.fsize);
    }
    return new FATFileHandle(fh);
}
    
int FATFileSystem::remove(const char *filename) {
    FRESULT res = f_unlink(filename);
    if (res) { 
        debug_if(FFS_DBG, "f_unlink() failed: %d\n", res);
        return -1;
    }
    return 0;
}

int FATFileSystem::format() {
	char n[64];
	sprintf(n, "%d:/", _fsid);
	FRESULT res = f_mkfs(n, 0, 512); // Logical drive number, Partitioning rule, Allocation unit size (bytes per cluster)
    if (res) {
        debug_if(FFS_DBG, "f_mkfs() failed: %d\n", res);
        return -1;
    }
    return 0;
}

DirHandle *FATFileSystem::opendir(const char *name) {
    FAT_DIR dir;
    FRESULT res = f_opendir(&dir, name);
    if (res != 0) {
        return NULL;
    }
    return new FATDirHandle(dir);
}


int FATFileSystem::closedir(DirHandle *dir) {
	if(dir)
	{
		return dir->closedir();
	}
	else
		return false;
}

int FATFileSystem::mkdir(const char *name, mode_t mode) {
    FRESULT res = f_mkdir(name);
    return res == 0 ? 0 : -1;
}
