/* ʸ�ᶭ���θ��Ф˻Ȥ��ǡ��� */
#ifndef _wordborder_h_included_
#define _wordborder_h_included_


#include <anthy/dic.h>
#include <anthy/alloc.h>
#include <anthy/segclass.h>
#include <anthy/depgraph.h>

struct splitter_context;

/*
 * meta_word�λ��Ѳ�ǽ�����å��Τ����
 */
enum mw_check {
  /* �ʤˤ⤻�� */
  MW_CHECK_NONE,
  /* mw->wl��̵������wl���Ȥ����� */
  MW_CHECK_SINGLE,
  MW_CHECK_BORDER,
  MW_CHECK_WRAP,
  MW_CHECK_OCHAIRE,
  MW_CHECK_NUMBER,
  MW_CHECK_COMPOUND
};

/*
 * ʸ������Τ������ɽ����
 * ��������Ϥޤ�meta_word, word_list�Υ��åȤ����
 */
struct char_node {
  int max_len;
  struct meta_word *mw;
  struct word_list *wl;
};

/*
 * ����ƥ�������μ�Ω��ʤɤξ��󡢺ǽ���Ѵ������򲡤����Ȥ���
 * ���ۤ����
 */
struct word_split_info_cache {
  struct char_node *cnode;

  /* ����å��幽�����˻Ȥ����� */
  /* ��������õ���Τ˻Ȥ� */
  int *seq_len;/* ��������Ϥޤ��Ĺ��ñ���Ĺ�� */
  /* ��Ƭ����õ���Τ˻Ȥ� */
  int *rev_seq_len;/* �����ǽ�����Ĺ��ñ���Ĺ�� */
  /* ʸ�ᶭ��context����Υ��ԡ� */
  int *seg_border;
  /* �����ǰ������Ӥ��ɤ��ä����饹 */
  enum seg_class* best_seg_class;
  /*  */
  struct meta_word **best_mw;
  /* �������� */
  allocator MwAllocator, WlAllocator;
};

/*
 * meta_word�ξ���
 */
enum mw_status {
  MW_STATUS_NONE,
  /* mw->mw1����Ȥ����äƤ��� */
  MW_STATUS_WRAPPED,
  /* mw-mw1��mw->mw2����Ϣ�� */
  MW_STATUS_COMBINED,
  /* ʣ����� */
  MW_STATUS_COMPOUND,
  /* ʣ���θġ���ʸ����礷�ư�Ĥ�ʸ��Ȥ��Ƹ������ */
  MW_STATUS_COMPOUND_PART,
  /* OCHAIRE�ؽ�������Ф� */
  MW_STATUS_OCHAIRE
};



/* metaword�μ���ˤ������ΰ㤤 (metaword.c) */
extern struct metaword_type_tab_ {
  enum metaword_type type;
  const char *name;
  enum mw_status status;
  enum mw_check check;
} anthy_metaword_type_tab[];

/*
 * 0: ��Ƭ��
 * 1: ��Ω����
 * 2: ������
 */
#define NR_PARTS 4
#define PART_PREFIX 0
#define PART_CORE 1
#define PART_POSTFIX 2
#define PART_DEPWORD 3

struct part_info {
  /* ����part��Ĺ�� */
  int from, len;
  /* �ʻ� */
  wtype_t wt;
  seq_ent_t seq;
  /* ���� */
  int freq;
  /* ��°�쥯�饹 */
  enum dep_class dc;
};

/*
 * word_list: ʸ������������
 * ��Ƭ�졢��Ω�졢�����졢��°���ޤ�
 */
struct word_list {
  /**/
  int from, len; /* ʸ������ */
  int is_compound; /* ʣ��줫�ɤ��� */

  /**/
  int dep_word_hash;
  int mw_features;
  /**/
  enum seg_class seg_class;
  enum constraint_stat can_use; /* �������ȶ����˸٤��äƤ��ʤ� */

  /* ���������뤿��ǤϤʤ��ơ���¿�ʽ����˻Ȥ��������� */
  int head_pos; /* lattice�����Ѥ��ʻ� */
  int tail_ct; /* meta_word�η���Ѥγ��ѷ� */

  /**/
  int last_part;
  struct part_info part[NR_PARTS];

  /* ����word_list���ä��ݤξ��� */
  int node_id; /* ��°�쥰��դθ������Ϥ�node��id*/

  /* Ʊ��from�����word_list�Υꥹ�� */
  struct word_list *next;
};


/* splitter.c */
#define SPLITTER_DEBUG_NONE 0
/* wordlist��ɽ�� */
#define SPLITTER_DEBUG_WL 1
/* metaword��ɽ�� */
#define SPLITTER_DEBUG_MW 2
/* lattice�� node��ɽ�� */
#define SPLITTER_DEBUG_LN 4
/* ��Ω��Υޥå������ʻ� */
#define SPLITTER_DEBUG_ID 8
/**/
#define SPLITTER_DEBUG_CAND 16

int anthy_splitter_debug_flags(void);


/* defined in wordseq.c */
/* ��Ω��ʹߤ���³�ν��� */
void anthy_scan_node(struct splitter_context *sc,
		     struct word_list *wl,
		     xstr *follow, int node);
int anthy_get_node_id_by_name(const char *name);
int anthy_init_depword_tab(void);
void anthy_quit_depword_tab(void);

/* depgraph.c */
int anthy_get_nr_dep_rule(void);
void anthy_get_nth_dep_rule(int, struct wordseq_rule *);

/* defined in wordlist.c */
void anthy_commit_word_list(struct splitter_context *, struct word_list *wl);
struct word_list *anthy_alloc_word_list(struct splitter_context *);
void anthy_print_word_list(struct splitter_context *, struct word_list *);
void anthy_make_word_list_all(struct splitter_context *);

/* defined in metaword.c */
void anthy_commit_meta_word(struct splitter_context *, struct meta_word *mw);
void anthy_make_metaword_all(struct splitter_context *);
void anthy_print_metaword(struct splitter_context *, struct meta_word *);

void anthy_mark_border_by_metaword(struct splitter_context* sc,
				   struct meta_word* mw);


/* defined in evalborder.c */
void anthy_eval_border(struct splitter_context *, int, int, int);

/* defined at lattice.c */
void anthy_mark_borders(struct splitter_context *sc, int from, int to);

/* defined at seg_class.c */
void anthy_set_seg_class(struct word_list* wl);

/* �ʻ�(anthy_init_splitter�ǽ���������) */
extern wtype_t anthy_wtype_noun;
extern wtype_t anthy_wtype_name_noun;
extern wtype_t anthy_wtype_num_noun;
extern wtype_t anthy_wtype_prefix;
extern wtype_t anthy_wtype_num_prefix;
extern wtype_t anthy_wtype_num_postfix;
extern wtype_t anthy_wtype_name_postfix;
extern wtype_t anthy_wtype_sv_postfix;
extern wtype_t anthy_wtype_a_tail_of_v_renyou;
extern wtype_t anthy_wtype_v_renyou;
extern wtype_t anthy_wtype_noun_tail;/* ����֤��ơפȤ� */
extern wtype_t anthy_wtype_n1;
extern wtype_t anthy_wtype_n10;

#endif
