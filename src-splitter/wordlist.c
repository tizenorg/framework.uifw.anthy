/*
 * ʸ��κǾ�ñ�̤Ǥ���wordlist��������
 *
 * anthy_make_word_list_all() 
 * ʸ��η�������������ʬʸ�������󤹤�
 *  �������η�ϩ����󤵤줿word_list��
 *  anthy_commit_word_list��splitter_context���ɲä����
 *
 * Funded by IPA̤Ƨ���եȥ�������¤���� 2002 2/27
 * Copyright (C) 2000-2006 TABATA Yusuke
 * Copyright (C) 2004-2006 YOSHIDA Yuichi
 * Copyright (C) 2000-2003 UGAWA Tomoharu
 *
 * $Id: wordlist.c,v 1.50 2002/11/17 14:45:47 yusuke Exp $
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>

#include <anthy/alloc.h>
#include <anthy/record.h>
#include <anthy/xstr.h>
#include <anthy/diclib.h>
#include <anthy/wtype.h>
#include <anthy/ruleparser.h>
#include <anthy/dic.h>
#include <anthy/splitter.h>
#include <anthy/feature_set.h>
#include "wordborder.h"

#define HF_THRESH 784

static void *weak_word_array;

/* �ǥХå��� */
void
anthy_print_word_list(struct splitter_context *sc,
		      struct word_list *wl)
{
  xstr xs;
  if (!wl) {
    printf("--\n");
    return ;
  }
  /* ��Ƭ�� */
  xs.len = wl->part[PART_CORE].from - wl->from;
  xs.str = sc->ce[wl->from].c;
  anthy_putxstr(&xs);
  printf(".");
  /* ��Ω�� */
  xs.len = wl->part[PART_CORE].len;
  xs.str = sc->ce[wl->part[PART_CORE].from].c;
  anthy_putxstr(&xs);
  printf(".");
  /* ������ */
  xs.len = wl->part[PART_POSTFIX].len;
  xs.str = sc->ce[wl->part[PART_CORE].from + wl->part[PART_CORE].len].c;
  anthy_putxstr(&xs);
  printf("-");
  /* ��°�� */
  xs.len = wl->part[PART_DEPWORD].len;
  xs.str = sc->ce[wl->part[PART_CORE].from +
		  wl->part[PART_CORE].len +
		  wl->part[PART_POSTFIX].len].c;
  anthy_putxstr(&xs);
  anthy_print_wtype(wl->part[PART_CORE].wt);
  printf(" %s%s\n", anthy_seg_class_name(wl->seg_class),
	 (wl->is_compound ? ",compound" : ""));
}

int
anthy_dep_word_hash(xstr *xs)
{
  return anthy_xstr_hash(xs) % WORD_HASH_MAX;
}

/** word_list����Ӥ��롢�޴���Τ���ʤΤǡ�
    ��̩����ӤǤ���ɬ�פ�̵�� */
static int
word_list_same(struct word_list *wl1, struct word_list *wl2)
{
  if (wl1->node_id != wl2->node_id ||
      wl1->from != wl2->from ||
      wl1->len != wl2->len ||
      wl1->mw_features != wl2->mw_features ||
      wl1->tail_ct != wl2->tail_ct ||
      wl1->part[PART_CORE].len != wl2->part[PART_CORE].len ||
      wl1->is_compound != wl2->is_compound ||
      !anthy_wtype_equal(wl1->part[PART_CORE].wt, wl2->part[PART_CORE].wt) ||
      wl1->head_pos != wl2->head_pos) {
    return 0;
  }
  if (wl1->part[PART_DEPWORD].dc != wl2->part[PART_DEPWORD].dc) {
    return 0;
  }
  /* Ʊ����Ƚ�� */
  return 1;
}

static void
set_features(struct word_list *wl)
{
  if (anthy_wtype_get_pos(wl->part[PART_CORE].wt) == POS_NOUN &&
      anthy_wtype_get_sv(wl->part[PART_CORE].wt)) {
    wl->mw_features |= MW_FEATURE_SV;
  }
  if (wl->part[PART_POSTFIX].len || wl->part[PART_PREFIX].len) {
    wl->mw_features |= MW_FEATURE_SUFFIX;
  }
  if (anthy_wtype_get_pos(wl->part[PART_CORE].wt) == POS_NUMBER) {
    wl->mw_features |= MW_FEATURE_NUM;
  }
  if (wl->part[PART_CORE].len == 1) {
    wl->mw_features |= MW_FEATURE_CORE1;
  }
  if (wl->part[PART_CORE].len == 0) {
    wl->mw_features |= MW_FEATURE_DEP_ONLY;
  }
  if (wl->part[PART_CORE].freq > HF_THRESH) {
    wl->mw_features |= MW_FEATURE_HIGH_FREQ;
  }
}

/** ��ä�word_list�Υ�������׻����Ƥ��饳�ߥåȤ��� */
void 
anthy_commit_word_list(struct splitter_context *sc,
		       struct word_list *wl)
{
  struct word_list *tmp;
  xstr xs;

  /* ��°�������word_list�ǡ�Ĺ��0�Τ��äƤ���Τ� */
  if (wl->len == 0) return;
  /**/
  wl->last_part = PART_DEPWORD;

  /**/
  set_features(wl);
  /* ʸ�ᶭ���θ����ǻ��Ѥ��륯�饹������ */
  anthy_set_seg_class(wl);
  /**/
  xs.len = wl->part[PART_DEPWORD].len;
  xs.str = sc->ce[wl->part[PART_POSTFIX].from + wl->part[PART_POSTFIX].len].c;
  wl->dep_word_hash = anthy_dep_word_hash(&xs);
  if (wl->part[PART_POSTFIX].len) {
    xs.len = wl->part[PART_POSTFIX].len;
    xs.str = sc->ce[wl->part[PART_POSTFIX].from].c;
  }

  /* Ʊ�����Ƥ�word_list���ʤ�����Ĵ�٤� */
  for (tmp = sc->word_split_info->cnode[wl->from].wl; tmp; tmp = tmp->next) {
    if (word_list_same(tmp, wl)) {
      return ;
    }
  }
  /* wordlist�Υꥹ�Ȥ��ɲ� */
  wl->next = sc->word_split_info->cnode[wl->from].wl;
  sc->word_split_info->cnode[wl->from].wl = wl;

  /* �ǥХå��ץ��� */
  if (anthy_splitter_debug_flags() & SPLITTER_DEBUG_WL) {
    anthy_print_word_list(sc, wl);
  }
}

struct word_list *
anthy_alloc_word_list(struct splitter_context *sc)
{
  return anthy_smalloc(sc->word_split_info->WlAllocator);
}

/* ��³�γ��Ѹ��������졢��ư����դ��� */
static void
make_following_word_list(struct splitter_context *sc,
			 struct word_list *tmpl)
{
  /* ����xs�ϼ�Ω�����θ�³��ʸ���� */
  xstr xs;
  xs.str = sc->ce[tmpl->from+tmpl->len].c;
  xs.len = sc->char_count - tmpl->from - tmpl->len;
  tmpl->part[PART_DEPWORD].from =
    tmpl->part[PART_POSTFIX].from + tmpl->part[PART_POSTFIX].len;

  if (tmpl->node_id >= 0) {
    /* ���̤�word_list */
    anthy_scan_node(sc, tmpl, &xs, tmpl->node_id);
  } else {
    /* ��Ω�줬�ʤ�word_list */
    struct wordseq_rule rule;
    struct word_list new_tmpl;
    int i;
    int nr_rule = anthy_get_nr_dep_rule();
    new_tmpl = *tmpl;
    /* ̾��35�θ��³���롼����Ф��� */
    for (i = 0; i < nr_rule; ++i) {
      anthy_get_nth_dep_rule(i, &rule);
      if (anthy_wtype_get_pos(rule.wt) == POS_NOUN
	  && anthy_wtype_get_scos(rule.wt) == SCOS_T35) {
	new_tmpl.part[PART_CORE].wt = rule.wt;
	new_tmpl.node_id = rule.node_id;
	new_tmpl.head_pos = anthy_wtype_get_pos(new_tmpl.part[PART_CORE].wt);
	anthy_scan_node(sc, &new_tmpl, &xs, new_tmpl.node_id);
      }
    }
  }
}

static void
push_part_back(struct word_list *tmpl, int len,
	       seq_ent_t se, wtype_t wt)
{
  tmpl->len += len;
  tmpl->part[PART_POSTFIX].len += len;
  tmpl->part[PART_POSTFIX].wt = wt;
  tmpl->part[PART_POSTFIX].seq = se;
  tmpl->last_part = PART_POSTFIX;
}

/* �������򤯤äĤ��� */
static void 
make_suc_words(struct splitter_context *sc,
	       struct word_list *tmpl)
{
  int i, right;

  wtype_t core_wt = tmpl->part[PART_CORE].wt;
  /* ���졢̾��������̾��Τ����줫����°����դ� */
  int core_is_num = 0;
  int core_is_name = 0;
  int core_is_sv_noun = 0;

  /* �ޤ������������դ���Ω�줫�����å����� */
  if (anthy_wtype_include(anthy_wtype_num_noun, core_wt)) {
    core_is_num = 1;
  }
  if (anthy_wtype_include(anthy_wtype_name_noun, core_wt)) {
    core_is_name = 1;
  }
  if (anthy_wtype_get_sv(core_wt)) {
    core_is_sv_noun = 1;
  }
  if (!core_is_num && !core_is_name && !core_is_sv_noun) {
    return ;
  }

  right = tmpl->part[PART_CORE].from + tmpl->part[PART_CORE].len;
  /* ��Ω��α�¦��ʸ������Ф��� */
  for (i = 1;
       i <= sc->word_split_info->seq_len[right];
       i++){
    xstr xs;
    seq_ent_t suc;
    xs.str = sc->ce[right].c;
    xs.len = i;
    suc = anthy_get_seq_ent_from_xstr(&xs, sc->is_reverse);
    if (anthy_get_seq_ent_pos(suc, POS_SUC)) {
      /* ��¦��ʸ�������°��ʤΤǡ���Ω����ʻ�ˤ��碌�ƥ����å� */
      struct word_list new_tmpl;
      if (core_is_num &&
	  anthy_get_seq_ent_wtype_freq(suc, anthy_wtype_num_postfix)) {
	new_tmpl = *tmpl;
	push_part_back(&new_tmpl, i, suc, anthy_wtype_num_postfix);
	make_following_word_list(sc, &new_tmpl);
      }
      if (core_is_name &&
	  anthy_get_seq_ent_wtype_freq(suc, anthy_wtype_name_postfix)) {
	new_tmpl = *tmpl;
	push_part_back(&new_tmpl, i, suc, anthy_wtype_name_postfix);
	make_following_word_list(sc, &new_tmpl);
      }
      if (core_is_sv_noun &&
	  anthy_get_seq_ent_wtype_freq(suc, anthy_wtype_sv_postfix)) {
	new_tmpl = *tmpl;
	push_part_back(&new_tmpl, i, suc, anthy_wtype_sv_postfix);
	make_following_word_list(sc, &new_tmpl);
      }
    }
  }
}

static void
push_part_front(struct word_list *tmpl, int len,
		seq_ent_t se, wtype_t wt)
{
  tmpl->from = tmpl->from - len;
  tmpl->len = tmpl->len + len;
  tmpl->part[PART_PREFIX].from = tmpl->from;
  tmpl->part[PART_PREFIX].len += len;
  tmpl->part[PART_PREFIX].wt = wt;
  tmpl->part[PART_PREFIX].seq = se;
}

/* ��Ƭ���򤯤äĤ��Ƥ����������򤯤äĤ��� */
static void
make_pre_words(struct splitter_context *sc,
	       struct word_list *tmpl)
{
  int i;
  wtype_t core_wt = tmpl->part[PART_CORE].wt;
  int core_is_num = 0;
  /* ��Ω��Ͽ��줫�� */
  if (anthy_wtype_include(anthy_wtype_num_noun, core_wt)) {
    core_is_num = 1;
  }
  /* ��Ƭ������󤹤� */
  for (i = 1; 
       i <= sc->word_split_info->rev_seq_len[tmpl->part[PART_CORE].from];
       i++) {
    seq_ent_t pre;
    /* ����xs�ϼ�Ω����������ʸ���� */
    xstr xs;
    xs.str = sc->ce[tmpl->part[PART_CORE].from - i].c;
    xs.len = i;
    pre = anthy_get_seq_ent_from_xstr(&xs, sc->is_reverse);
    if (anthy_get_seq_ent_pos(pre, POS_PRE)) {
      struct word_list new_tmpl;
      if (core_is_num &&
	  anthy_get_seq_ent_wtype_freq(pre, anthy_wtype_num_prefix)) {
	new_tmpl = *tmpl;
	push_part_front(&new_tmpl, i, pre, anthy_wtype_num_prefix);
	make_following_word_list(sc, &new_tmpl);
	/* ���ξ����������⤯�äĤ��� */
	make_suc_words(sc, &new_tmpl);
      }/* else if (anthy_get_seq_ent_wtype_freq(pre, anthy_wtype_prefix)) {
	new_tmpl = *tmpl;
	push_part_front(&new_tmpl, i, pre, anthy_wtype_prefix);
	make_following_word_list(sc, &new_tmpl);
	}*/
    }
  }
}

/* wordlist���������� */
static void
setup_word_list(struct word_list *wl, int from, int len,
		int is_compound, int is_weak)
{
  int i;
  wl->from = from;
  wl->len = len;
  wl->is_compound = is_compound;
  /* part��������������� */
  for (i = 0; i < NR_PARTS; i++) {
    wl->part[i].from = 0;
    wl->part[i].len = 0;
    wl->part[i].wt = anthy_wt_none;
    wl->part[i].seq = 0;
    wl->part[i].freq = 1;/* ���٤��㤤ñ��Ȥ��Ƥ��� */
    wl->part[i].dc = DEP_NONE;
  }
  /* ��Ω��Υѡ��Ȥ����� */
  wl->part[PART_CORE].from = from;
  wl->part[PART_CORE].len = len;
  /**/
  wl->mw_features = MW_FEATURE_NONE;
  wl->node_id = -1;
  wl->last_part = PART_CORE;
  wl->head_pos = POS_NONE;
  wl->tail_ct = CT_NONE;
  if (is_weak) {
    wl->mw_features |= MW_FEATURE_WEAK_SEQ;
  }
}

/*
 * ������Ω����Ф��ơ���Ƭ��������������°����դ�����Τ�
 * ʸ��θ���(=word_list)�Ȥ���cache���ɲä���
 */
static void
make_word_list(struct splitter_context *sc,
	       seq_ent_t se,
	       int from, int len,
	       int is_compound,
	       int is_weak)
{
  struct word_list tmpl;
  struct wordseq_rule rule;
  int nr_rule = anthy_get_nr_dep_rule();
  int i;

  /* �ƥ�ץ졼�Ȥν���� */
  setup_word_list(&tmpl, from, len, is_compound, is_weak);
  tmpl.part[PART_CORE].seq = se;

  /* �ƥ롼��˥ޥå����뤫��� */
  for (i = 0; i < nr_rule; ++i) {
    int freq;
    anthy_get_nth_dep_rule(i, &rule);
    if (!is_compound) {
      freq = anthy_get_seq_ent_wtype_freq(se, rule.wt);
    } else {
      freq = anthy_get_seq_ent_wtype_compound_freq(se, rule.wt);
    }

    if (freq) {
      /* ��Ω����ʻ�Ϥ��Υ롼��ˤ��äƤ��� */
      if (anthy_splitter_debug_flags() & SPLITTER_DEBUG_ID) {
	/* �ʻ�ɽ�ΥǥХå���*/
	xstr xs;
	xs.str = sc->ce[tmpl.part[PART_CORE].from].c;
	xs.len = tmpl.part[PART_CORE].len;
	anthy_putxstr(&xs);
	printf(" freq=%d rule_id=%d node_id=%d\n",
	       freq, i, rule.node_id);
      }
      /* ���ܤ����롼��ξ����ž������ */
      tmpl.part[PART_CORE].wt = rule.wt;
      tmpl.part[PART_CORE].freq = freq;
      tmpl.node_id = rule.node_id;
      tmpl.head_pos = anthy_wtype_get_pos(tmpl.part[PART_CORE].wt);

      /**/
      tmpl.part[PART_POSTFIX].from =
	tmpl.part[PART_CORE].from +
	tmpl.part[PART_CORE].len;
      /**/
      if (anthy_wtype_get_pos(rule.wt) == POS_NOUN ||
	  anthy_wtype_get_pos(rule.wt) == POS_NUMBER) {
	/* ��Ƭ������������̾�졢����ˤ����դ��ʤ����Ȥˤ��Ƥ��� */
	make_pre_words(sc, &tmpl);
	make_suc_words(sc, &tmpl);
      }
      /* ��Ƭ����������̵���ǽ����ư���Ĥ��� */
      make_following_word_list(sc, &tmpl);
    }
  }
}

static void
make_dummy_head(struct splitter_context *sc)
{
  struct word_list tmpl;
  setup_word_list(&tmpl, 0, 0, 0, 0);
  tmpl.part[PART_CORE].seq = 0;
  tmpl.part[PART_CORE].wt = anthy_wtype_noun;

  tmpl.head_pos = anthy_wtype_get_pos(tmpl.part[PART_CORE].wt);
  make_suc_words(sc, &tmpl);
}

static int
compare_hash(const void *kp, const void *cp)
{
  const int *h = kp;
  const int *c = cp;
  return (*h) - ntohl(*c);
}

static int
check_weak(xstr *xs)
{
  const int *array = (int *)weak_word_array;
  int nr;
  int h;
  if (!array) {
    return 0;
  }
  nr = ntohl(array[1]);
  h = anthy_xstr_hash(xs);
  if (bsearch(&h, &array[16], nr,
	      sizeof(int), compare_hash)) {
    return 1;
  }
  return 0;
}

/* ����ƥ����Ȥ����ꤵ�줿ʸ�������ʬʸ���󤫤����Ƥ�word_list����󤹤� */
void 
anthy_make_word_list_all(struct splitter_context *sc)
{
  int i, j;
  xstr xs;
  seq_ent_t se;
  struct depword_ent {
    struct depword_ent *next;
    int from, len;
    int is_compound;
    int is_weak;
    seq_ent_t se;
  } *head, *de;
  struct word_split_info_cache *info;
  allocator de_ator;

  weak_word_array = anthy_file_dic_get_section("weak_words");

  info = sc->word_split_info;
  head = NULL;
  de_ator = anthy_create_allocator(sizeof(struct depword_ent), 0);

  xs.str = sc->ce[0].c;
  xs.len = sc->char_count;
  anthy_gang_load_dic(&xs, sc->is_reverse);

  /* ���Ƥμ�Ω������ */
  /* ���������Υ롼�� */
  for (i = 0; i < sc->char_count ; i++) {
    int search_len = sc->char_count - i;
    int search_from = 0;
    if (search_len > 30) {
      search_len = 30;
    }

    /* ʸ����Ĺ�Υ롼��(Ĺ��������) */
    for (j = search_len; j > search_from; j--) {
      /* seq_ent��������� */
      xs.len = j;
      xs.str = sc->ce[i].c;
      se = anthy_get_seq_ent_from_xstr(&xs, sc->is_reverse);

      /* ñ��Ȥ���ǧ���Ǥ��ʤ� */
      if (!se) {
	continue;
      }

      /* �ơ���ʬʸ����ñ��ʤ����Ƭ������������
	 ����Ĺ��Ĵ�٤ƥޡ������� */
      if (j > info->seq_len[i] &&
	  anthy_get_seq_ent_pos(se, POS_SUC)) {
	info->seq_len[i] = j;
      }
      if (j > info->rev_seq_len[i + j] &&
	  anthy_get_seq_ent_pos(se, POS_PRE)) {
	info->rev_seq_len[i + j] = j;
      }

      /* ȯ��������Ω���ꥹ�Ȥ��ɲ� */
      if (anthy_get_seq_ent_indep(se) &&
	  /* ʣ����̵�����䤬���뤳�Ȥ��ǧ */
	  anthy_has_non_compound_ents(se)) {
	de = (struct depword_ent *)anthy_smalloc(de_ator);
	de->from = i;
	de->len = j;
	de->se = se;
	de->is_compound = 0;
	de->is_weak = check_weak(&xs);

	de->next = head;
	head = de;
      }
      /* ȯ������ʣ����ꥹ�Ȥ��ɲ� */
      if (anthy_has_compound_ents(se)) {
	de = (struct depword_ent *)anthy_smalloc(de_ator);
	de->from = i;
	de->len = j;
	de->se = se;
	de->is_compound = 1;
	de->is_weak = 0;

	de->next = head;
	head = de;
      }
    }
  }

  /* ȯ��������Ω�����Ƥ��Ф�����°��ѥ�����θ��� */
  for (de = head; de; de = de->next) {
    make_word_list(sc, de->se, de->from, de->len,
		   de->is_compound, de->is_weak);
  }

    /* ��Ω���̵��word_list */
  for (i = 0; i < sc->char_count; i++) {
    struct word_list tmpl;
    setup_word_list(&tmpl, i, 0, 0, 0);
    if (i == 0) {
      make_following_word_list(sc, &tmpl);
    } else {
      int type = anthy_get_xchar_type(*sc->ce[i - 1].c);
      if ((type & (XCT_CLOSE | XCT_SYMBOL)) &&
	  !(type & XCT_PUNCTUATION)) {
	/* �������ʳ��ε��� */
	make_following_word_list(sc, &tmpl);
      }
    }
  }  

  /* ��Ƭ��0ʸ���μ�Ω����դ��� */
  make_dummy_head(sc);

  anthy_free_allocator(de_ator);
}
