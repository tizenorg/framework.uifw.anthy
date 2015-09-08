/*
 * ʸ��ʸ���split����splitter
 *
 * ʸ��ζ����򸡽Ф���
 *  anthy_init_split_context() ʬ���ѤΥ���ƥ����Ȥ��ä�
 *  anthy_mark_border() ʬ��򤷤�
 *  anthy_release_split_context() ����ƥ����Ȥ��������
 *
 *  anthy_commit_border() ���ߥåȤ��줿���Ƥ��Ф��Ƴؽ��򤹤�
 *
 * Funded by IPA̤Ƨ���եȥ�������¤���� 2001 9/22
 *
 * Copyright (C) 2004 YOSHIDA Yuichi
 * Copyright (C) 2000-2004 TABATA Yusuke
 * Copyright (C) 2000-2001 UGAWA Tomoharu
 *
 * $Id: splitter.c,v 1.48 2002/11/18 11:39:18 yusuke Exp $
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
#include <string.h>

#include <anthy/alloc.h>
#include <anthy/record.h>
#include <anthy/splitter.h>
#include <anthy/logger.h>
#include "wordborder.h"

#define MAX_EXPAND_PAIR_ENTRY_COUNT 1000

static int splitter_debug_flags;

/**/
wtype_t anthy_wtype_noun;
wtype_t anthy_wtype_name_noun;
wtype_t anthy_wtype_num_noun;
wtype_t anthy_wtype_prefix;
wtype_t anthy_wtype_num_prefix;
wtype_t anthy_wtype_num_postfix;
wtype_t anthy_wtype_name_postfix;
wtype_t anthy_wtype_sv_postfix;
wtype_t anthy_wtype_a_tail_of_v_renyou;
wtype_t anthy_wtype_v_renyou;
wtype_t anthy_wtype_noun_tail;/* ����֤��ơפȤ� */
wtype_t anthy_wtype_n1;
wtype_t anthy_wtype_n10;


/** make_word_cache�Ǻ�������ʸ�������������
 */
static void
release_info_cache(struct splitter_context *sc)
{
  struct word_split_info_cache *info = sc->word_split_info;

  anthy_free_allocator(info->MwAllocator);
  anthy_free_allocator(info->WlAllocator);
  free(info->cnode);
  free(info->seq_len);
  free(info->rev_seq_len);
  free(info);
}

static void
metaword_dtor(void *p)
{
  struct meta_word *mw = (struct meta_word*)p;
  if (mw->cand_hint.str) {
    free(mw->cand_hint.str);
  }
}


static void
alloc_char_ent(xstr *xs, struct splitter_context *sc)
{
  int i;
 
  sc->char_count = xs->len;
  sc->ce = (struct char_ent*)
    malloc(sizeof(struct char_ent)*(xs->len + 1));
  for (i = 0; i <= xs->len; i++) {
    sc->ce[i].c = &xs->str[i];
    sc->ce[i].seg_border = 0;
    sc->ce[i].initial_seg_len = 0;
    sc->ce[i].best_seg_class = SEG_HEAD;
    sc->ce[i].best_mw = NULL;
  }
 
  /* ����ξü��ʸ��ζ����Ǥ��� */
  sc->ce[0].seg_border = 1;
  sc->ce[xs->len].seg_border = 1;
}

/*  �����ǳ��ݤ������Ƥ�release_info_cache�ǲ�������� 
 */
static void
alloc_info_cache(struct splitter_context *sc)
{
  int i;
  struct word_split_info_cache *info;

  /* ����å���Υǡ�������� */
  sc->word_split_info = malloc(sizeof(struct word_split_info_cache));
  info = sc->word_split_info;
  info->MwAllocator = anthy_create_allocator(sizeof(struct meta_word), metaword_dtor);
  info->WlAllocator = anthy_create_allocator(sizeof(struct word_list), 0);
  info->cnode =
    malloc(sizeof(struct char_node) * (sc->char_count + 1));

  info->seq_len = malloc(sizeof(int) * (sc->char_count + 1));
  info->rev_seq_len = malloc(sizeof(int) * (sc->char_count + 1));

  /* ��ʸ������ǥå������Ф��ƽ������Ԥ� */
  for (i = 0; i <= sc->char_count; i++) {
    info->seq_len[i] = 0;
    info->rev_seq_len[i] = 0;
    info->cnode[i].wl = NULL;
    info->cnode[i].mw = NULL;
    info->cnode[i].max_len = 0;
  }
}

/** ������ƤӽФ����wordsplitter�Υȥåץ�٥�δؿ� */
void
anthy_mark_border(struct splitter_context *sc,
		  int from, int from2, int to)
{
  int i;
  struct word_split_info_cache *info;

  /* sanity check */
  if ((to - from) <= 0) {
    return ;
  }

  /* �����ޡ����Ѥ�lattice�θ������Ѥ����륯�饹�Ѥ��ΰ����� */
  info = sc->word_split_info;
  info->seg_border = alloca(sizeof(int)*(sc->char_count + 1));
  info->best_seg_class = alloca(sizeof(enum seg_class)*(sc->char_count + 1));
  info->best_mw = alloca(sizeof(struct meta_word*)*(sc->char_count + 1));
  for (i = 0; i < sc->char_count + 1; ++i) {
    info->seg_border[i] = sc->ce[i].seg_border;
    info->best_seg_class[i] = sc->ce[i].best_seg_class;
    info->best_mw[i] = sc->ce[i].best_mw;
  }

  /* ��������ꤹ�� */
  anthy_eval_border(sc, from, from2, to);

  for (i = from; i < to; ++i) {
    sc->ce[i].seg_border = info->seg_border[i];
    sc->ce[i].best_seg_class = info->best_seg_class[i];
    sc->ce[i].best_mw = info->best_mw[i];
  }
}

/* ʸ�᤬���礵�줿�Τǡ������ؽ����� */
static void
proc_expanded_segment(struct splitter_context *sc,
		      int from, int len)
{
  int initial_len = sc->ce[from].initial_seg_len;
  int i, nr;
  xstr from_xs, to_xs, *xs;

  from_xs.str = sc->ce[from].c;
  from_xs.len = initial_len;
  to_xs.str = sc->ce[from].c;
  to_xs.len = len;
  if (anthy_select_section("EXPANDPAIR", 1) == -1) {
    return ;
  }
  if (anthy_select_row(&from_xs, 1) == -1) {
    return ;
  }
  nr = anthy_get_nr_values();
  for (i = 0; i < nr; i ++) {
    xs = anthy_get_nth_xstr(i);
    if (!xs || !anthy_xstrcmp(xs, &to_xs)) {
      /* ���ˤ��� */
      return ;
    }
  }
  anthy_set_nth_xstr(nr, &to_xs);
  anthy_truncate_section(MAX_EXPAND_PAIR_ENTRY_COUNT);
}

/* ʸ��Υޡ����ȸ�����ؽ����� */
void
anthy_commit_border(struct splitter_context *sc, int nr_segments,
		    struct meta_word **mw, int *seg_len)
{
  int i, from = 0;

  /* ���Ф���ʸ�� */
  for (i = 0; i < nr_segments; i++) {
    /* ���줾���ʸ����Ф��� */

    int len = seg_len[i];
    int initial_len = sc->ce[from].initial_seg_len;
    int real_len = 0;
    int l2;

    if (!initial_len || from + initial_len == sc->char_count) {
      /* �����϶����ǤϤʤ� */
      goto tail;
    }
    l2 = sc->ce[from + initial_len].initial_seg_len;
    if (initial_len + l2 > len) {
      /* �٤�ʸ���ޤ�ۤɳ��礵�줿�櫓�ǤϤʤ� */
      goto tail;
    }
    if (mw[i]) {
      real_len = mw[i]->len;
    }
    if (real_len <= initial_len) {
      goto tail;
    }
    /* ����ʸ���ޤ�Ĺ���˳�ĥ���줿ʸ�᤬���ߥåȤ��줿 */
    proc_expanded_segment(sc, from, real_len);
  tail:
    from += len;
  }
}

int
anthy_splitter_debug_flags(void)
{
  return splitter_debug_flags;
}

void
anthy_init_split_context(xstr *xs, struct splitter_context *sc, int is_reverse)
{
  alloc_char_ent(xs, sc);
  alloc_info_cache(sc);
  sc->is_reverse = is_reverse;
  /* ���Ƥ���ʬʸ���������å����ơ�ʸ��θ������󤹤�
     word_list�������Ƥ���metaword�������� */
  anthy_lock_dic();
  anthy_make_word_list_all(sc);
  anthy_unlock_dic();
  anthy_make_metaword_all(sc);

}

void
anthy_release_split_context(struct splitter_context *sc)
{
  if (sc->word_split_info) {
    release_info_cache(sc);
    sc->word_split_info = 0;
  }
  if (sc->ce) {
    free(sc->ce);
    sc->ce = 0;
  }
}

/** splitter���Τν������Ԥ� */
int
anthy_init_splitter(void)
{
  /* �ǥХå��ץ��Ȥ����� */
  char *en = getenv("ANTHY_ENABLE_DEBUG_PRINT");
  char *dis = getenv("ANTHY_DISABLE_DEBUG_PRINT");
  splitter_debug_flags = SPLITTER_DEBUG_NONE;
  if (!dis && en && strlen(en)) {
    char *fs = getenv("ANTHY_SPLITTER_PRINT");
    if (fs) {
      if (strchr(fs, 'w')) {
	splitter_debug_flags |= SPLITTER_DEBUG_WL;
      }
      if (strchr(fs, 'm')) {
	splitter_debug_flags |= SPLITTER_DEBUG_MW;
      }
      if (strchr(fs, 'l')) {
	splitter_debug_flags |= SPLITTER_DEBUG_LN;
      }
      if (strchr(fs, 'i')) {
	splitter_debug_flags |= SPLITTER_DEBUG_ID;
      }
      if (strchr(fs, 'c')) {
	splitter_debug_flags |= SPLITTER_DEBUG_CAND;
      }
    }
  }
  /* ��°�쥰��դν���� */
  if (anthy_init_depword_tab()) {
    anthy_log(0, "Failed to init dependent word table.\n");
    return -1;
  }
  /**/
  anthy_wtype_noun = anthy_init_wtype_by_name("̾��35");
  anthy_wtype_name_noun = anthy_init_wtype_by_name("��̾");
  anthy_wtype_num_noun = anthy_init_wtype_by_name("����");
  anthy_wtype_a_tail_of_v_renyou = anthy_init_wtype_by_name("���ƻ첽������");
  anthy_wtype_v_renyou = anthy_init_wtype_by_name("ư��Ϣ�ѷ�");
  anthy_wtype_noun_tail = anthy_init_wtype_by_name("̾�첽������");
  anthy_wtype_prefix = anthy_init_wtype_by_name("̾����Ƭ��");
  anthy_wtype_num_prefix = anthy_init_wtype_by_name("����Ƭ��");
  anthy_wtype_num_postfix = anthy_init_wtype_by_name("��������");
  anthy_wtype_name_postfix = anthy_init_wtype_by_name("��̾������");
  anthy_wtype_sv_postfix = anthy_init_wtype_by_name("����������");
  anthy_wtype_n1 = anthy_init_wtype_by_name("����1");
  anthy_wtype_n10 = anthy_init_wtype_by_name("����10");
  return 0;
}

void
anthy_quit_splitter(void)
{
  anthy_quit_depword_tab();
}
