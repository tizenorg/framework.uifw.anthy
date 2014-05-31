/* mmap����ݲ����� */
#ifndef _filemap_h_included_
#define _filemap_h_included_

/* ������map���줿�ե�����Υϥ�ɥ� */
struct filemapping;

struct filemapping *anthy_mmap(const char *fn, int wr);
void *anthy_mmap_address(struct filemapping *m);
int anthy_mmap_size(struct filemapping *m);
int anthy_mmap_is_writable(struct filemapping *m);
void anthy_munmap(struct filemapping *m);

#endif
