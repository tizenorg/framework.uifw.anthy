/*
 * mem_dic ����Υ���å����Ԥ�
 *
 * ����å�����ɤߤ�ʸ����ȵ��Ѵ��Ѥ��Υե饰(is_reverse)��
 * ��Ĥ򥭡��Ȥ�������롣
 *
 * Copyright (C) 2000-2007 TABATA Yusuke
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
#include <stdlib.h>

#include <anthy/alloc.h>
#include "dic_main.h"
#include "mem_dic.h"

static allocator mem_dic_ator;

static void
dic_ent_dtor(void *p)
{
  struct dic_ent *de = p;
  if (de->str.str) {
    free(de->str.str);
  }
}

static void
seq_ent_dtor(void *p)
{
  struct seq_ent *seq = p;
  int i;
  /**/
  for (i = 0; i < seq->nr_dic_ents; i++) {
    anthy_sfree(seq->md->dic_ent_allocator, seq->dic_ents[i]);
  }
  if (seq->nr_dic_ents) {
    free(seq->dic_ents);
  }
  /**/
  free(seq->str.str);
}

static void
mem_dic_dtor(void *p)
{
  struct mem_dic * md = p;
  anthy_free_allocator(md->seq_ent_allocator);
  anthy_free_allocator(md->dic_ent_allocator);
}

/** xstr���б�����seq_ent����ݤ��� */
static struct seq_ent *
alloc_seq_ent_by_xstr(struct mem_dic * md, xstr *x, int is_reverse)
{
  struct seq_ent *se;
  se = (struct seq_ent *)anthy_smalloc(md->seq_ent_allocator);
  if (is_reverse) {
    se->seq_type = ST_REVERSE;
  } else {
    se->seq_type = ST_NONE;
  }
  se->md = md;
  se->str.len = x->len;
  /**/
  se->nr_dic_ents = 0;
  se->dic_ents = NULL;
  /**/
  se->nr_compound_ents = 0;

  se->str.str = anthy_xstr_dup_str(x);
  return se;
}

/* �ϥå���ؿ����Ȥꤢ�����Ƥ��ȡ� */
static int
hash_function(xstr *xs)
{
  if (xs->len) {
    return xs->str[0]% HASH_SIZE;
  }
  return 0;
}

/** xstr���б�����seq_ent���֤� */
struct seq_ent *
anthy_mem_dic_alloc_seq_ent_by_xstr(struct mem_dic * md, xstr *xs,
				    int is_reverse)
{
  struct seq_ent *se;
  int h;
  /* ����å���ˤ���Ф�����֤� */
  se = anthy_mem_dic_find_seq_ent_by_xstr(md, xs, is_reverse);
  if (se) {
    return se;
  }
  /* ����å���ˤ�̵���ΤǺ�� */
  se = alloc_seq_ent_by_xstr(md, xs, is_reverse);

  /* mem_dic��ˤĤʤ� */
  h = hash_function(xs);
  se->next = md->seq_ent_hash[h];
  md->seq_ent_hash[h] = se;

  return se;
}

static int
compare_seq_ent(struct seq_ent *seq, xstr *xs, int is_reverse)
{
  /* �ޤ����ɤ��餫�����Ѵ��ѤΥ���ȥ꤫������å� */
  if (seq->seq_type & ST_REVERSE) {
    if (!is_reverse) {
      return 1;
    }
  } else {
    if (is_reverse) {
      return 1;
    }
  }
  /* ����ʸ�������� */
  return anthy_xstrcmp(&seq->str, xs);
}

/*** mem_dic���椫��ʸ������б�����seq_ent*���������
 * */
struct seq_ent *
anthy_mem_dic_find_seq_ent_by_xstr(struct mem_dic * md, xstr *xs,
				   int is_reverse)
{
  struct seq_ent *sn;
  int h;
  h = hash_function(xs);
  for (sn = md->seq_ent_hash[h]; sn; sn = sn->next) {
    if (!compare_seq_ent(sn, xs, is_reverse)){
      return sn;
    }
  }
  return 0;
}

void
anthy_mem_dic_release_seq_ent(struct mem_dic * md, xstr *xs, int is_reverse)
{
  struct seq_ent *sn;
  struct seq_ent **sn_prev_p;
  int h;

  h = hash_function(xs);
  sn_prev_p = &md->seq_ent_hash[h];
  for (sn = md->seq_ent_hash[h]; sn; sn = sn->next) {
    if (!compare_seq_ent(sn, xs, is_reverse)){
      *sn_prev_p = sn->next;
      anthy_sfree(md->seq_ent_allocator, sn);
      return;
    } else {
      sn_prev_p = &sn->next;
    }
  }
}

/** seq_ent��dic_ent���ɲä��� */
void
anthy_mem_dic_push_back_dic_ent(struct seq_ent *se, int is_compound,
				xstr *xs, wtype_t wt,
				const char *wt_name, int freq, int feature)
{
  struct dic_ent *de;
  de = anthy_smalloc(se->md->dic_ent_allocator);
  de->type = wt;
  de->wt_name = wt_name;
  de->freq = freq;
  de->feature = feature;
  de->order = 0;
  de->is_compound = is_compound;
  de->str.len = xs->len;
  de->str.str = anthy_xstr_dup_str(xs);

  if (is_compound) {
    se->nr_compound_ents ++;
  }

  /* order��׻����� */
  if (se->nr_dic_ents > 0) {
    struct dic_ent *prev_de = se->dic_ents[se->nr_dic_ents-1];
    if (anthy_wtype_equal(prev_de->type, de->type) &&
	prev_de->freq > de->freq) {
      de->order = prev_de->order + 1;
    }
  }

  /* ������ɲä��� */
  se->nr_dic_ents ++;
  se->dic_ents = realloc(se->dic_ents,
			 sizeof(struct dic_ent *)*se->nr_dic_ents);
  se->dic_ents[se->nr_dic_ents-1] = de;
}

struct mem_dic *
anthy_create_mem_dic(void)
{
  int i;
  struct mem_dic *md;

  md = anthy_smalloc(mem_dic_ator);
  for (i = 0; i < HASH_SIZE; i++) {
    md->seq_ent_hash[i] = NULL;
  }
  
  md->seq_ent_allocator = 
    anthy_create_allocator(sizeof(struct seq_ent),
			   seq_ent_dtor);
  md->dic_ent_allocator =
    anthy_create_allocator(sizeof(struct dic_ent),
			   dic_ent_dtor);

  return md;
}

void
anthy_release_mem_dic(struct mem_dic * d)
{
  anthy_sfree(mem_dic_ator, d);
}

void
anthy_init_mem_dic(void)
{
  mem_dic_ator = anthy_create_allocator(sizeof(struct mem_dic),
					mem_dic_dtor);
}

void
anthy_quit_mem_dic(void)
{
  anthy_free_allocator(mem_dic_ator);
}
