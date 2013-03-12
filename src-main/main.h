#ifndef _main_h_included_
#define _main_h_included_

#include <anthy/xstr.h>
#include <anthy/dic.h>
#include <anthy/splitter.h>
#include <anthy/segment.h>
#include <anthy/ordering.h>
#include <anthy/prediction.h>

/* 
   ͽ¬�Ѵ��θ���Υ���å���
 */
struct prediction_cache {
  /* ͽ¬����ʸ���� */
  xstr str;
  /* ͽ¬���줿����ο� */
  int nr_prediction;
  /* ͽ¬���줿���� */
  struct prediction_t* predictions;
};

/** Anthy���Ѵ�����ƥ�����
 * �Ѵ����ʸ����ʤɤ����äƤ���
 */
struct anthy_context {
  /** ����ƥ����Ȥλ���ʸ���� */
  xstr str;
  /** ʸ��Υꥹ�� */
  struct segment_list seg_list;
  /** ���񥻥å���� */
  dic_session_t dic_session;
  /** splitter�ξ��� */
  struct splitter_context split_info;
  /** ������¤��ؤ����� */
  struct ordering_context_wrapper ordering_info;
  /** ͽ¬���� */
  struct prediction_cache prediction;
  /** ���󥳡��ǥ��� */
  int encoding;
  /** ���Ѵ��Υ⡼�� */
  int reconversion_mode;
};


/* context.c */
void anthy_init_contexts(void);
void anthy_quit_contexts(void);
void anthy_init_personality(void);
void anthy_quit_personality(void);
int anthy_do_set_personality(const char *id);
struct anthy_context *anthy_do_create_context(int);
int anthy_do_context_set_str(struct anthy_context *c, xstr *x, int is_reverse);
void anthy_do_reset_context(struct anthy_context *c);
void anthy_do_release_context(struct anthy_context *c);

void anthy_do_resize_segment(struct anthy_context *c,int nth,int resize);

int anthy_do_set_prediction_str(struct anthy_context *c, xstr *x);
void anthy_release_segment_list(struct anthy_context *ac);
void anthy_save_history(const char *fn, struct anthy_context *ac);

/* for debug */
void anthy_do_print_context(struct anthy_context *c, int encoding);


#endif
/* �ʤ�٤����ؤ�ե�åȤˤ����� */
