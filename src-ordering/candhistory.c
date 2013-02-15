/*
 * ����������Ф���
 *
 *
 * �����ɤߤ����� ����A ����B ����A ����A ����A
 * �Ǥ��ä��Ȥ����褦�ʾ�����Ȥ˸���Υ�������������롣
 *
 * Copyright (C) 2006-2007 TABATA Yusuke
 *
 */
#include <stdlib.h>

#include <anthy/segment.h>
#include <anthy/record.h>
#include "sorter.h"

#define HISTORY_DEPTH 8
#define MAX_HISTORY_ENTRY 200

/** ʸ��Υ��ߥåȤ�������ɲä��� */
static void
learn_cand_history(struct seg_ent *seg)
{
  int nr, i;

  if (anthy_select_section("CAND_HISTORY", 1)) {
    return ;
  }
  if (anthy_select_row(&seg->str, 1)) {
    return ;
  }
  /* ���եȤ��� */
  nr = anthy_get_nr_values();
  nr ++;
  if (nr > HISTORY_DEPTH) {
    nr = HISTORY_DEPTH;
  }
  for (i = nr - 1; i > 0; i--) {
    xstr *xs = anthy_get_nth_xstr(i - 1);
    anthy_set_nth_xstr(i, xs);
  }
  /* 0���ܤ����� */
  anthy_set_nth_xstr(0, &seg->cands[seg->committed]->str);
  anthy_mark_row_used();
}

static void
learn_suffix_history(struct seg_ent *seg)
{
  int i;
  struct cand_ent *cand = seg->cands[seg->committed];
  if (anthy_select_section("SUFFIX_HISTORY", 1)) {
    return ;
  }
  for (i = 0; i < cand->nr_words; i++) {
    struct cand_elm *elm = &cand->elm[i];
    xstr xs;
    if (elm->nth == -1) {
      continue;
    }
    if (anthy_wtype_get_pos(elm->wt) != POS_SUC) {
      continue;
    }
    if (anthy_select_row(&elm->str, 1)) {
      continue;
    }
    if (anthy_get_nth_dic_ent_str(elm->se, &elm->str, elm->nth, &xs)) {
      continue;
    }
    anthy_set_nth_xstr(0, &xs);
    free(xs.str);
  }
}

/** ������ƤФ��ؿ� 
 * ������ɲä��� */
void
anthy_learn_cand_history(struct segment_list *sl)
{
  int i, nr = 0;
  for (i = 0; i < sl->nr_segments; i++) {
    struct seg_ent *seg = anthy_get_nth_segment(sl, i);
    xstr *xs = &seg->str;
    if (seg->committed < 0) {
      continue;
    }
    if (anthy_select_row(xs, 0)) {
      if (seg->committed == 0) {
	/* ����Υ���ȥ̵꤬���ơ����ߥåȤ��줿�������Ƭ�Τ�ΤǤ���Хѥ� */
	continue;
      }
    }
    /**/
    learn_cand_history(seg);
    learn_suffix_history(seg);
    nr ++;
  }
  if (nr > 0) {
    if (!anthy_select_section("CAND_HISTORY", 1)) {
      anthy_truncate_section(MAX_HISTORY_ENTRY);
    }
    if (!anthy_select_section("SUFFIX_HISTORY", 1)) {
      anthy_truncate_section(MAX_HISTORY_ENTRY);
    }
  }
}

/* �����ߤƸ���νŤߤ�׻����� */
static int
get_history_weight(xstr *xs)
{
  int i, nr = anthy_get_nr_values();
  int w = 0;
  for (i = 0; i < nr; i++) {
    xstr *h = anthy_get_nth_xstr(i);
    if (!h) {
      continue;
    }
    if (!anthy_xstrcmp(xs, h)) {
      w++;
      if (i == 0) {
	/* ľ���˳��ꤵ�줿��ΤˤϹ⤤������*/
	w += (HISTORY_DEPTH / 2);
      }
    }
  }
  return w;
}

static void
reorder_by_candidate(struct seg_ent *se)
{
  int i, primary_score;
  /**/
  if (anthy_select_section("CAND_HISTORY", 1)) {
    return ;
  }
  if (anthy_select_row(&se->str, 0)) {
    return ;
  }
  /* �Ǥ�ɾ���ι⤤���� */
  primary_score = se->cands[0]->score;
  /**/
  for (i = 0; i < se->nr_cands; i++) {
    struct cand_ent *ce = se->cands[i];
    int weight = get_history_weight(&ce->str);
    ce->score += primary_score / (HISTORY_DEPTH /2) * weight;
  }
  anthy_mark_row_used();
}

/* �������γؽ���Ŭ�Ѥ��� */
static void
reorder_by_suffix(struct seg_ent *se)
{
  int i, j;
  int delta = 0;
  int top_cand = -1;
  if (anthy_select_section("SUFFIX_HISTORY", 0)) {
    return ;
  }
  /* �Ƹ��� */
  for (i = 0; i < se->nr_cands; i++) {
    struct cand_ent *ce = se->cands[i];
    /* ������������ñ�� */
    for (j = 0; j < ce->nr_words; j++) {
      struct cand_elm *elm = &ce->elm[j];
      xstr xs;
      if (elm->nth == -1) {
	continue;
      }
      if (anthy_wtype_get_pos(elm->wt) != POS_SUC) {
	continue;
      }
      /* �Ѵ�����ʸ����򥭡��˸��� */
      if (anthy_select_row(&elm->str, 0)) {
	continue;
      }
      /* �Ѵ����ʸ�������� */
      if (anthy_get_nth_dic_ent_str(elm->se, &elm->str, elm->nth, &xs)) {
	continue;
      }
      /* �������ʸ�������Ӥ��� */
      if (anthy_xstrcmp(&xs, anthy_get_nth_xstr(0))) {
	free(xs.str);
	continue;
      }
      /**/
      if (top_cand < 0) {
	top_cand = i;
      }
      if (delta == 0) {
	delta = (se->cands[top_cand]->score - ce->score) + 1;
      }
      ce->score += delta;
      free(xs.str);
    }
  }
}

/* ����ǲ������� */
void
anthy_reorder_candidates_by_history(struct seg_ent *se)
{
  reorder_by_candidate(se);
  reorder_by_suffix(se);
}
