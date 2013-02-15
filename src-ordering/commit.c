/*
 * ����(���ߥå�)��ν����򤹤롣
 * �Ƽ�γؽ�������ƤӽФ�
 *
 * anthy_proc_commit() ����������ƤФ��
 */
#include <stdlib.h>
#include <time.h>

#include <anthy/ordering.h>
#include <anthy/record.h>
#include <anthy/splitter.h>
#include <anthy/segment.h>
#include "sorter.h"

#define MAX_OCHAIRE_ENTRY_COUNT 100
#define MAX_OCHAIRE_LEN 32
#define MAX_PREDICTION_ENTRY 100

#define MAX_UNKNOWN_WORD 100

/* �򴹤��줿�����õ�� */
static void
learn_swapped_candidates(struct segment_list *sl)
{
  int i;
  struct seg_ent *seg;
  for (i = 0; i < sl->nr_segments; i++) {
    seg = anthy_get_nth_segment(sl, i);
    if (seg->committed != 0) {
      /* �ǽ�θ���(0����)�Ǥʤ�����(seg->committed����)�����ߥåȤ��줿 */
      anthy_swap_cand_ent(seg->cands[0],
			  seg->cands[seg->committed]);
    }
  }
  anthy_cand_swap_ageup();
}

/* Ĺ�����Ѥ�ä�ʸ����ѹ�����Ф��� */
static void
learn_resized_segment(struct splitter_context *sc,
		      struct segment_list *sl)
		      
{
  int i;
  struct meta_word **mw
    = alloca(sizeof(struct meta_word*) * sl->nr_segments);
  int *len_array
    = alloca(sizeof(int) * sl->nr_segments);

  /* ��ʸ���Ĺ���������meta_word��������Ѱդ��� */
  for (i = 0; i < sl->nr_segments; i++) {
    struct seg_ent *se = anthy_get_nth_segment(sl, i);
    mw[i] = se->cands[se->committed]->mw;
    len_array[i] = se->str.len;
  }

  anthy_commit_border(sc, sl->nr_segments, mw, len_array);
}

/* Ĺ�����Ѥ�ä�ʸ����ѹ������Ф��� */
static void
clear_resized_segment(struct splitter_context *sc,
		      struct segment_list *sl)
{
  int *mark, i, from;
  struct seg_ent *seg;
  mark = alloca(sizeof(int)*sc->char_count);
  for (i = 0; i < sc->char_count; i++) {
    mark[i] = 0;
  }
  /* �ºݤ˳��ꤵ�줿ʸ���Ĺ����ޡ������� */
  from = 0;
  for (i = 0; i < sl->nr_segments; i++) {
    seg = anthy_get_nth_segment(sl, i);
    mark[from] = seg->len;
    from = from + seg->len;
  }
  for (i = 0; i < sc->char_count; i++) {
    int len = sc->ce[i].initial_seg_len;
    /* �ǽ��Ĺ���ȳ��ꤵ�줿Ĺ�����ۤʤ�С�
       �Ȥ��ʤ��ä�̤�θ�β�ǽ�������� */
    if (len && len != mark[i]) {
      xstr xs;
      xs.str = sc->ce[i].c;
      xs.len = len;
      anthy_forget_unused_unknown_word(&xs);
    }
  }
  if (!anthy_select_section("UNKNOWN_WORD", 0)) {
    anthy_truncate_section(MAX_UNKNOWN_WORD);
  }
}

/* record�ˤ�������ؽ��η�̤�񤭹��� */
static void
commit_ochaire(struct seg_ent *seg, int count, xstr* xs)
{
  int i;
  if (xs->len >= MAX_OCHAIRE_LEN) {
    return ;
  }
  if (anthy_select_row(xs, 1)) {
    return ;
  }
  anthy_set_nth_value(0, count);
  for (i = 0; i < count; i++, seg = seg->next) {
    anthy_set_nth_value(i * 2 + 1, seg->len);
    anthy_set_nth_xstr(i * 2 + 2, &seg->cands[seg->committed]->str);
  }
}

/* record���ΰ�����󤹤뤿��ˡ���������ؽ��Υͥ��ƥ��֤�
   ����ȥ��ä� */
static void
release_negative_ochaire(struct splitter_context *sc,
			 struct segment_list *sl)
{
  int start, len;
  xstr xs;
  (void)sl;
  /* �Ѵ����ΤҤ餬��ʸ���� */
  xs.len = sc->char_count;
  xs.str = sc->ce[0].c;

  /* xs����ʬʸ������Ф��� */
  for (start = 0; start < xs.len; start ++) {
    for (len = 1; len <= xs.len - start && len < MAX_OCHAIRE_LEN; len ++) {
      xstr part;
      part.str = &xs.str[start];
      part.len = len;
      if (anthy_select_row(&part, 0) == 0) {
	anthy_release_row();
      }
    }
  }
}

/* ��������ؽ���Ԥ� */
static void
learn_ochaire(struct splitter_context *sc,
	      struct segment_list *sl)
{
  int i;
  int count;

  if (anthy_select_section("OCHAIRE", 1)) {
    return ;
  }

  /* ��������ؽ��Υͥ��ƥ��֤ʥ���ȥ��ä� */
  release_negative_ochaire(sc, sl);

  /* ��������ؽ��򤹤� */
  for (count = 2; count <= sl->nr_segments && count < 5; count++) {
    /* 2ʸ��ʾ��Ĺ����ʸ������Ф��� */

    for (i = 0; i <= sl->nr_segments - count; i++) {
      struct seg_ent *head = anthy_get_nth_segment(sl, i);
      struct seg_ent *s;
      xstr xs;
      int j;
      xs = head->str;
      if (xs.len < 2 && count < 3) {
	/* ���ڤ��ʸ���ؽ����뤳�Ȥ��򤱤롢
	 * �����ø���heuristics */
	continue;
      }
      /* ʸ�����������ʸ������� */
      for (j = 1, s = head->next; j < count; j++, s = s->next) {
	xs.len += s->str.len;
      }
      /**/
      commit_ochaire(head, count, &xs);
    }
  }
  if (anthy_select_section("OCHAIRE", 1)) {
    return ;
  }
  anthy_truncate_section(MAX_OCHAIRE_ENTRY_COUNT);
}

static int
learn_prediction_str(xstr *idx, xstr *xs)
{
  int nr_predictions;
  int i;
  time_t t = time(NULL);
  if (anthy_select_row(idx, 1)) {
    return 0;
  }
  nr_predictions = anthy_get_nr_values();

  /* ��������ˤ�����ϥ����ॹ����פ������� */
  for (i = 0; i < nr_predictions; i += 2) {
    xstr *log = anthy_get_nth_xstr(i + 1);
    if (!log) {
      continue;
    }
    if (anthy_xstrcmp(log, xs) == 0) {
      anthy_set_nth_value(i, t);
      break;
    }
  }

  /* �ʤ������������ɲ� */
  if (i == nr_predictions) {
    anthy_set_nth_value(nr_predictions, t);
    anthy_set_nth_xstr(nr_predictions + 1, xs);      
    anthy_mark_row_used();
    return 1;
  }
  anthy_mark_row_used();
  return 0;
}

static void
learn_prediction(struct segment_list *sl)
{
  int i;
  int added = 0;
  if (anthy_select_section("PREDICTION", 1)) {
    return ;
  }
  for (i = 0; i < sl->nr_segments; i++) {
    struct seg_ent *seg = anthy_get_nth_segment(sl, i);
    xstr *xs = &seg->cands[seg->committed]->str;

    if (seg->committed < 0) {
      continue;
    }
    if (learn_prediction_str(&seg->str, xs)) {
      added = 1;
    }
  }
  if (added) {
    anthy_truncate_section(MAX_PREDICTION_ENTRY);
  }
}

static void
learn_unknown(struct segment_list *sl)
{
  int i;
  for (i = 0; i < sl->nr_segments; i++) {
    struct seg_ent *seg = anthy_get_nth_segment(sl, i);
    struct cand_ent *ce = seg->cands[seg->committed];
    if (ce->nr_words == 0) {
      anthy_add_unknown_word(&seg->str, &ce->str);
    }
  }
}

void
anthy_do_commit_prediction(xstr *src, xstr *xs)
{
  if (anthy_select_section("PREDICTION", 1)) {
    return ;
  }
  learn_prediction_str(src, xs);
}

void
anthy_proc_commit(struct segment_list *sl,
		  struct splitter_context *sc)
{
  /* �Ƽ�γؽ���Ԥ� */
  learn_swapped_candidates(sl);
  learn_resized_segment(sc, sl);
  clear_resized_segment(sc, sl);
  learn_ochaire(sc, sl);
  learn_prediction(sl);
  learn_unknown(sl);
  anthy_learn_cand_history(sl);
}
