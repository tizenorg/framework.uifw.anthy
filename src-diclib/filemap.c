/* mmap����ݲ�����
 *
 * *Unix�ϤΥ����ƥॳ����򥽡�����˻��餵�ʤ�����
 * *����Ū�ˤϰ�ĤΥե������ʣ������Ū��map���뤳�Ȥ��θ
 *
 * Copyright (C) 2005 TABATA Yusuke
 *
 */
/*
  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307  USA
 */
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>

#include <anthy/filemap.h>
#include <anthy/logger.h>

struct filemapping {
  /* �񤭹��ߥ⡼�� */
  int wr;
  /* mmap�������ɥ쥹 */
  void *ptr;
  /* mmap�����ΰ��Ĺ�� */
  size_t size;
};

struct filemapping *
anthy_mmap(const char *fn, int wr)
{
  int fd;
  void *ptr;
  int r;
  struct filemapping *m;
  struct stat st;
  int prot;
  int flags;
  mode_t mode;

  if (wr) {
    prot = PROT_READ | PROT_WRITE;
    flags = O_RDWR;
    mode = S_IRUSR | S_IWUSR;
  } else {
    prot = PROT_READ;
    flags = O_RDONLY;
    mode = S_IRUSR;
  }

  fd = open(fn, flags, mode);

  if (fd == -1) {
    anthy_log(0, "Failed to open (%s).\n", fn);
    return NULL;
  }

  r = fstat(fd, &st);
  if (r == -1) {
    anthy_log(0, "Failed to stat() (%s).\n", fn);
    close(fd);
    return NULL;
  }
  if (st.st_size == 0) {
    anthy_log(0, "Failed to mmap 0size file (%s).\n", fn);
    close(fd);
    return NULL;
  }

  ptr = mmap(NULL, st.st_size, prot, MAP_SHARED, fd, 0);
  close(fd);
  if (ptr == MAP_FAILED) {
    anthy_log(0, "Failed to mmap() (%s).\n", fn);
    return NULL;
  }

  /* mmap�����������ΤǾ�����֤� */
  m = malloc(sizeof(struct filemapping));
  m->size = st.st_size;
  m->ptr = ptr;
  m->wr = wr;

  return m;
}

void *
anthy_mmap_address(struct filemapping *m)
{
  if (!m) {
    return NULL;
  }
  return m->ptr;
}

int
anthy_mmap_size(struct filemapping *m)
{
  if (!m) {
    return 0;
  }
  return m->size;
}

int
anthy_mmap_is_writable(struct filemapping *m)
{
  if (!m) {
    return 0;
  }
  return m->wr;
}

void
anthy_munmap(struct filemapping *m)
{
  if (!m) {
    return ;
  }
  munmap(m->ptr, m->size);
  free(m);
}
