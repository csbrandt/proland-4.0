/*
 * Proland: a procedural landscape rendering library.
 * Copyright (c) 2008-2011 INRIA
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Proland is distributed under a dual-license scheme.
 * You can obtain a specific license from Inria: proland-licensing@inria.fr.
 */

/*
 * Authors: Eric Bruneton, Antoine Begault, Guillaume Piolat.
 */

#ifndef _MFS_
#define _MFS_

/*
--------------------------------------------------------------------------------
-   Module      :   mem_file.c
-   Description :   A general purpose library for manipulating a memory area
-                   as if it were a file.
-                   mfs_ stands for memory file system.
-   Author      :   Mike Johnson - Banctec AB 03/07/96
-
--------------------------------------------------------------------------------
*/

/*

Copyright (c) 1996 Mike Johnson
Copyright (c) 1996 BancTec AB

Permission to use, copy, modify, distribute, and sell this software
for any purpose is hereby granted without fee, provided
that (i) the above copyright notices and this permission notice appear in
all copies of the software and related documentation, and (ii) the names of
Mike Johnson and BancTec may not be used in any advertising or
publicity relating to the software.

THE SOFTWARE IS PROVIDED "AS-IS" AND WITHOUT WARRANTY OF ANY KIND,
EXPRESS, IMPLIED OR OTHERWISE, INCLUDING WITHOUT LIMITATION, ANY
WARRANTY OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.

IN NO EVENT SHALL MIKE JOHNSON OR BANCTEC BE LIABLE FOR
ANY SPECIAL, INCIDENTAL, INDIRECT OR CONSEQUENTIAL DAMAGES OF ANY KIND,
OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER OR NOT ADVISED OF THE POSSIBILITY OF DAMAGE, AND ON ANY THEORY OF
LIABILITY, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
OF THIS SOFTWARE.

*/

typedef struct {
    char *buf;        /* Memory for open buf */
    long  buf_off;    /* File pointer for each buf */
    long  buf_size;   /* Count of bytes allocated for each buf */
    int   buf_mode;   /* Mode of buffer (r, w, a) */
    bool  buf_open;
} mfs_file;

PROLAND_API int mfs_open (void *ptr, int size, char *mode, mfs_file *fd);
PROLAND_API int mfs_lseek (mfs_file *fd, int offset, int whence);
PROLAND_API int mfs_read (mfs_file *fd, void *buf, int size);
PROLAND_API int mfs_write (mfs_file *fd, void *buf, int size);
PROLAND_API int mfs_size (mfs_file *fd);
PROLAND_API int mfs_map (mfs_file *fd, char **addr, size_t *len);
PROLAND_API int mfs_unmap (mfs_file *fd);
PROLAND_API int mfs_close (mfs_file *fd);

#endif
