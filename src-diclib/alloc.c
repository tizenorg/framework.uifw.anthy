/*
 * ��¤�Υ�������
 * Funded by IPA̤Ƨ���եȥ�������¤���� 2001 8/15
 *
 * $Id: alloc.c,v 1.12 2002/05/15 11:21:10 yusuke Exp $
 *
 * Copyright (C) 2005 YOSHIDA Yuichi
 * Copyright (C) 2000-2005 TABATA Yusuke, UGAWA Tomoharu
 * Copyright (C) 2002, 2005 NIIBE Yutaka
 *
 * dtor: destructor
 * 
 * �ڡ�����Υե꡼��chunk��ñ�����ꥹ�Ȥ˷Ѥ���Ƥ���
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <anthy/alloc.h>
#include <anthy/logger.h>

/**/
#define PAGE_MAGIC 0x12345678
#define PAGE_SIZE 2048

/* �ڡ��������̤ι�ס��ǥХå��λ������Ѥ��� */
static int nr_pages;

/* page��Υ��֥������Ȥ�ɽ�����֥������� */
struct chunk {
  void *storage[1];
};
#define CHUNK_HEADER_SIZE ((size_t)&((struct chunk *)0)->storage)
/* CPU�⤷���ϡ�OS�μ���ˤ�ä��׵ᤵ��륢�饤���� */
#define CHUNK_ALIGN (sizeof(double))

/*
 * page��storage��ˤ� 
 * max_obj = (PAGE_SIZE - PAGE_HEADER_SIZE) / (size + CHUNK_HEADER_SIZE)�Ĥ�
 * ����åȤ����롣���Τ���use_count�ĤΥ���åȤ�free_list�ˤĤʤ��äƤ��롢
 * �⤷���ϻ�����Ǥ��롣
 */
struct page {
  int magic;
  struct page *prev, *next;
};


#define PAGE_HEADER_SIZE (sizeof(struct page))
#define PAGE_AVAIL(p) ((unsigned char*)p + sizeof(struct page))
#define PAGE_STORAGE(a, p) (((unsigned char *)p) + (a->storage_offset))
#define PAGE_CHUNK(a, p, i) (struct chunk*)(&PAGE_STORAGE(a, p)[((a->size) + CHUNK_HEADER_SIZE) * (i)])


/**/
struct allocator_priv {
  /* ��¤�ΤΥ����� */
  int size;
  /* �ڡ����������뤳�Ȥ��Ǥ��륪�֥������Ȥο� */
  int max_num;
  /* 
     �ºݤΥǡ�������Ǽ����Ϥ����Υ��ե��å� 
     �ڡ�����Τ���������ˤ��б�������Υǡ������Ȥ��Ƥ��뤫�ɤ�����0/1��ɽ��
     �ӥåȥޥåפ�����
   */
  int storage_offset;
  /* ����allocator�����Ѥ��Ƥ���ڡ����Υꥹ�� */
  struct page page_list;
  /* allocator�Υꥹ�� */
  struct allocator_priv *next;
  /* sfree�����ݤ˸ƤФ�� */
  void (*dtor)(void *);
};

static struct allocator_priv *allocator_list;

static int bit_test(unsigned char* bits, int pos)
{
  /*
     bit_get�Ȥۤ�Ʊ������bit != 0�λ���0�ʳ����֤����Ȥ����ݾڤ��ʤ�
   */
  return bits[pos >> 3] & (1 << (7 - (pos & 0x7)));
}


static int bit_set(unsigned char* bits, int pos, int bit)
{
  unsigned char filter = 1 << (7 - (pos & 0x7));
  if (bit == 0) {
    return bits[pos >> 3] &= ~filter;
  } else {
    return bits[pos >> 3] |= filter;
  }
}


static struct chunk *
get_chunk_address(void *s)
{
  return (struct chunk *)
    ((unsigned long)s - CHUNK_HEADER_SIZE);
}

static struct page *
alloc_page(struct allocator_priv *ator)
{
  struct page *p;
  unsigned char* avail;
    
  p = malloc(PAGE_SIZE);
  if (!p) {
    return NULL;
  }

  p->magic = PAGE_MAGIC;
  avail = PAGE_AVAIL(p);
  memset(avail, 0, (ator->max_num >> 3) + 1);
  return p;
}

static struct chunk *
get_chunk_from_page(allocator a, struct page *p)
{
  int i;

  int num = a->max_num;
  unsigned char* avail = PAGE_AVAIL(p);

  for (i = 0; i < num; ++i) {
    if (bit_test(avail, i) == 0) {
      bit_set(avail, i, 1);
      return PAGE_CHUNK(a, p, i);
    }
  }
  return NULL;  
}

static int
roundup_align(int num)
{
  num = num + (CHUNK_ALIGN - 1);
  num /= CHUNK_ALIGN;
  num *= CHUNK_ALIGN;
  return num;
}

static int
calc_max_num(int size)
{
  int area, bits;
  /* �ӥåȿ��Ƿ׻�
   * ��̩�ʺ�Ŭ��ǤϤʤ�
   */
  area = (PAGE_SIZE - PAGE_HEADER_SIZE - CHUNK_ALIGN) * 8;
  bits = (size + CHUNK_HEADER_SIZE) * 8 + 1;
  return (int)(area / bits);
}

allocator
anthy_create_allocator(int size, void (*dtor)(void *))
{
  allocator a;
size=roundup_align(size);
  if (size > (int)(PAGE_SIZE - PAGE_HEADER_SIZE - CHUNK_HEADER_SIZE)) {
    anthy_log(0, "Fatal error: too big allocator is requested.\n");
    exit(1);
  }
  a = malloc(sizeof(*a));
  if (!a) {
    anthy_log(0, "Fatal error: Failed to allocate memory.\n");
    exit(1);
  }
  a->size = size;
  a->max_num = calc_max_num(size); 
  a->storage_offset = roundup_align(sizeof(struct page) + a->max_num / 8 + 1);
  /*printf("size=%d max_num=%d offset=%d area=%d\n", size, a->max_num, a->storage_offset, size*a->max_num + a->storage_offset);*/
  a->dtor = dtor;
  a->page_list.next = &a->page_list;
  a->page_list.prev = &a->page_list;
  a->next = allocator_list;
  allocator_list = a;
  return a;
}

static void
anthy_free_allocator_internal(allocator a)
{
  struct page *p, *p_next;

  /* �ƥڡ����Υ����������� */
  for (p = a->page_list.next; p != &a->page_list; p = p_next) {
    unsigned char* avail = PAGE_AVAIL(p);
    int i;

    p_next = p->next;
    if (a->dtor) {
      for (i = 0; i < a->max_num; i++) {
	if (bit_test(avail, i)) {
	  struct chunk *c;

	  bit_set(avail, i, 0);
	  c = PAGE_CHUNK(a, p, i);
	  a->dtor(c->storage);
	}
      }
    }
    free(p);
    nr_pages--;
  }
  free(a);
}

void
anthy_free_allocator(allocator a)
{
  allocator a0, *a_prev_p;

  /* �ꥹ�Ȥ���a���������Ǥ��դ��� */
  a_prev_p = &allocator_list;
  for (a0 = allocator_list; a0; a0 = a0->next) {
    if (a == a0)
      break;
    else
      a_prev_p = &a0->next;
  }
  /* a��ꥹ�Ȥ��鳰�� */
  *a_prev_p = a->next;

  anthy_free_allocator_internal(a);
}

void *
anthy_smalloc(allocator a)
{
  struct page *p;
  struct chunk *c;

  /* �����Ƥ�ڡ����򤵤��� */
  for (p = a->page_list.next; p != &a->page_list; p = p->next) {
    c = get_chunk_from_page(a, p);
    if (c) {
      return c->storage;
    }
  }
  /* �ڡ������äơ���󥯤��� */
  p = alloc_page(a);
  if (!p) {
    anthy_log(0, "Fatal error: Failed to allocate memory.\n");
    return NULL;
  }
  nr_pages++;

  p->next = a->page_list.next;
  p->prev = &a->page_list;
  a->page_list.next->prev = p;
  a->page_list.next = p;
  /* ���ľ�� */
  return anthy_smalloc(a);
}

void
anthy_sfree(allocator a, void *ptr)
{
  struct chunk *c = get_chunk_address(ptr);
  struct page *p;
  int index;
  /* �ݥ��󥿤δޤޤ��ڡ�����õ�� */
  for (p = a->page_list.next; p != &a->page_list; p = p->next) {
    if ((unsigned long)p < (unsigned long)c &&
	(unsigned long)c < (unsigned long)p + PAGE_SIZE) {
      break;
    }
  }

  /* sanity check */
  if (!p || p->magic != PAGE_MAGIC) {
    anthy_log(0, "sfree()ing Invalid Object\n");
    abort();
  }

  /* �ڡ�����β����ܤΥ��֥������Ȥ������ */
  index = ((unsigned long)c - (unsigned long)PAGE_STORAGE(a, p)) /
    (a->size + CHUNK_HEADER_SIZE);  
  bit_set(PAGE_AVAIL(p), index, 0);

  /* �ǥ��ȥ饯����Ƥ� */
  if (a->dtor) {
    a->dtor(ptr);
  }
}

void
anthy_quit_allocator(void)
{
  allocator a, a_next;
  for (a = allocator_list; a; a = a_next) {
    a_next = a->next;
    anthy_free_allocator_internal(a);
  }
  allocator_list = NULL;
}
