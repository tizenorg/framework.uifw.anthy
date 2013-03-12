/*
 * ����θ򴹤Υҥ��ȥ��������롣
 *
 * anthy_swap_cand_ent() �ǳؽ�����
 * anthy_proc_swap_candidate() �ǳؽ���̤��Ѥ���
 *
 *  ����ü���פȤ��������ȥåפ˽Ф��ơ���Ȫ���פǳ��ꤵ�줿����
 *  ��Ω����:����ü��->����Ȫ��
 *    ����ĤΥ���ȥ���ɲä���
 *
 */
#include <stdlib.h>

#include <anthy/record.h>
#include <anthy/segment.h>
/* for OCHAIRE_SCORE */
#include <anthy/splitter.h>
#include "sorter.h"

#define MAX_INDEP_PAIR_ENTRY 100

/* ����μ�Ω������ؽ����� */
static void
learn_swap_cand_indep(struct cand_ent *o, struct cand_ent *n)
{
  xstr os, ns;
  int res;
  int o_idx = o->core_elm_index;
  int n_idx = n->core_elm_index;

  /* ��Ω������ޤ�ʸ�ᤷ���ؽ����ʤ� */
  if (o_idx < 0 || n_idx < 0) {
    return ;
  }
  if (o->elm[o_idx].str.len != n->elm[n_idx].str.len) {
    return ;
  }
  if (o->elm[o_idx].nth == -1 || n->elm[n_idx].nth == -1) {
    return ;
  }
  res = anthy_get_nth_dic_ent_str(o->elm[o_idx].se, &o->elm[o_idx].str,
				  o->elm[o_idx].nth, &os);
  if (res) {
    return ;
  }
  res = anthy_get_nth_dic_ent_str(n->elm[n_idx].se, &n->elm[n_idx].str,
				  n->elm[n_idx].nth, &ns);
  if (res) {
    free(os.str);
    return ;
  }
  if (anthy_select_section("INDEPPAIR", 1) == 0) {
    if (anthy_select_row(&os, 1) == 0) {
      anthy_set_nth_xstr(0, &ns);
    }
  }
  free(os.str);
  free(ns.str);
}

/*
 * ����o ��Ф�����n �����ߥåȤ��줿�Τ�
 * o -> n ��record�˥��åȤ���
 */
void
anthy_swap_cand_ent(struct cand_ent *o, struct cand_ent *n)
{
  if (o == n) {
    /* Ʊ������ */
    return ;
  }
  if (n->flag & CEF_USEDICT) {
    /* ���㼭�񤫤�ФƤ������� */
    return ;
  }
  /* ��Ω���� */
  learn_swap_cand_indep(o, n);
}


/*
 * �Ѵ�������������������¤٤����֤Ǻ�ͥ��θ�������
 * �롼�פν���ʤɤ�Ԥ�
 */
static xstr *
prepare_swap_candidate(xstr *target)
{
  xstr *xs, *n;
  if (anthy_select_row(target, 0) == -1) {
    return NULL;
  }
  xs = anthy_get_nth_xstr(0);
  if (!xs) {
    return NULL;
  }
  /* ������ -> xs �Ȥʤ�Τ�ȯ�� */
  anthy_mark_row_used();
  if (anthy_select_row(xs, 0) != 0){
    /* xs -> �� */
    return xs;
  }
  /* xs -> n */
  n = anthy_get_nth_xstr(0);
  if (!n) {
    return NULL;
  }

  if (!anthy_xstrcmp(target, n)) {
    /* ������ -> xs -> n �� n = ������Υ롼�� */
    anthy_select_row(target, 0);
    anthy_release_row();
    anthy_select_row(xs, 0);
    anthy_release_row();
    /* ������ -> xs ��ä��ơ��򴹤�ɬ�פ�̵�� */
    return NULL;
  }
  /* ������ -> xs -> n �� n != ������ʤΤ�
   * ������ -> n������
   */
  if (anthy_select_row(target, 0) == 0){
    anthy_set_nth_xstr(0, n);
  }
  return n;
}

#include <src-worddic/dic_ent.h>

/*
 * ��Ω��Τ�
 */
static void
proc_swap_candidate_indep(struct seg_ent *se)
{
  xstr *xs;
  xstr key;
  int i;
  int core_elm_idx;
  int res;
  struct cand_elm *core_elm;

  core_elm_idx = se->cands[0]->core_elm_index;
  if (core_elm_idx < 0) {
    return ;
  }

  /* 0���ܤθ����ʸ�������Ф� */
  core_elm = &se->cands[0]->elm[core_elm_idx];
  if (core_elm->nth < 0) {
    return ;
  }
  res = anthy_get_nth_dic_ent_str(core_elm->se,
				  &core_elm->str,
				  core_elm->nth,
				  &key);
  if (res) {
    return ;
  }

  /**/
  anthy_select_section("INDEPPAIR", 1);
  xs = prepare_swap_candidate(&key);
  free(key.str);
  if (!xs) {
    return ;
  }

  /* ������ -> xs �ʤΤ� xs�θ����õ�� */
  for (i = 1; i < se->nr_cands; i++) {
    if (se->cands[i]->nr_words == se->cands[0]->nr_words &&
	se->cands[i]->core_elm_index == core_elm_idx) {
      xstr cand;
      res = anthy_get_nth_dic_ent_str(se->cands[i]->elm[core_elm_idx].se,
				      &se->cands[i]->elm[core_elm_idx].str,
				      se->cands[i]->elm[core_elm_idx].nth,
				      &cand);
      if (res == 0 &&
	  !anthy_xstrcmp(&cand, xs)) {
	free(cand.str);
	/* �ߤĤ����ΤǤ��θ���Υ������򥢥å� */
	se->cands[i]->score = se->cands[0]->score + 1;
	return ;
      }
      free(cand.str);
    }
  }
}

/*
 * �Ѵ�������������������¤٤����֤Ǻ�ͥ��θ�������
 */
void
anthy_proc_swap_candidate(struct seg_ent *seg)
{
  if (NULL == seg->cands) { /* ����⤷���ϳؽ��ǡ���������Ƥ��������к� */
    return;
  }

  if (seg->cands[0]->score >= OCHAIRE_SCORE) {
    /* cands[0] �����̤���������äƤ��� */
    return ;
  }
  if (seg->cands[0]->flag & CEF_USEDICT) {
    return ;
  }
  /**/
  proc_swap_candidate_indep(seg);
}

/* ����򴹤θŤ�����ȥ��ä� */
void
anthy_cand_swap_ageup(void)
{
  if (anthy_select_section("INDEPPAIR", 0) == 0) {
    anthy_truncate_section(MAX_INDEP_PAIR_ENTRY);
  }
}
