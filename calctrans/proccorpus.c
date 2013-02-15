/*
 * �����ѥ��Ȥʤ�ʸ�Ϥ��ɤ�ǡ�ʸ���Ĺ����Ĵ������
 * �����ǲ��Ϥη�̤���Ϥ���
 *
 * ���Ϸ����ˤĤ���
 *  �ޤ����̤�Ԥä�ʸ�᤬�ǽ��Ĺ���ǽ��Ϥ����
 *  ���˳�ʸ�����(�����)��ä����䡢����������ν�Ǿ������Ϥ���
 *
 *
 * Copyright (C) 2006-2007 TABATA Yusuke
 *
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <anthy/convdb.h>

static int verbose;

/* ʸ���Ĺ������ʸ�ˤ��碌�� */
static int
trim_segment(anthy_context_t ac, struct conv_res *cr,
	     int nth, char *seg)
{
  int len = strlen(seg);
  int resized = 0;
  (void)cr;

  while (1) {
    char seg_buf[1024];
    int cur_len;

    anthy_get_segment(ac, nth, NTH_UNCONVERTED_CANDIDATE, seg_buf, 1024);
    cur_len = strlen(seg_buf);
    if (len == cur_len) {
      return 1;
    }
    if (!resized) {
      resized = 1;
      /* ��������ʸ��ξ����ɽ������ */
      print_size_miss_segment_info(ac, nth);
    }
    if (len > cur_len) {
      anthy_resize_segment(ac, nth, 1);
    } else {
      anthy_resize_segment(ac, nth, -1);
    }
  }
  return 0;
}

/*
 * nth���ܤ�ʸ��Ǹ���seg��õ���Ƴ��ꤹ��
 */
static int
find_candidate(anthy_context_t ac, struct conv_res *cr,
	       int nth, char *seg)
{
  char seg_buf[1024];
  int i;
  struct anthy_segment_stat ass;

  if (seg[0] == '~') {
    /* ����ߥ��Υޡ�����~�פ򥹥��åפ��� */
    seg++;
    cr->cand_check[nth] = 1;
  }

  anthy_get_segment_stat(ac, nth, &ass);
  for (i = 0; i < ass.nr_candidate; i++) {
    anthy_get_segment(ac, nth, i, seg_buf, 1024);
    if (!strcmp(seg_buf, seg)) {
      /* ���פ������򸫤Ĥ����Τǳ��ꤹ�� */
      anthy_commit_segment(ac, nth, i);
      return 0;
    }
  }
  return 0;
}

/* '|' ��ʸ��˶��ڤ�줿ʸ����γ�ʸ��������fn��Ƥ� */
static int
for_each_segment(anthy_context_t ac, struct conv_res *cr,
		 const char *res_str,
		 int (*fn)(anthy_context_t ac, struct conv_res *cr,
			   int nth, char *seg))
{
  char *str, *cur, *cur_seg;
  int nth;
  if (!res_str) {
    return 0;
  }

  str = strdup(res_str);
  cur = str;
  cur ++;
  cur_seg = cur;
  nth = 0;
  while ((cur = strchr(cur, '|'))) {
    *cur = 0;
    /**/
    if (fn) {
      fn(ac, cr, nth, cur_seg);
    }
    /**/
    nth ++;
    cur ++;
    cur_seg = cur;
  }

  free(str);
  
  return 1;
}

static void
proc_sentence(anthy_context_t ac, struct conv_res *cr)
{
  int i;
  struct anthy_conv_stat acs;
  /*printf("(%s)\n", cr->src_str);*/
  anthy_set_string(ac, cr->src_str);
  /* ʸ���Ĺ����Ĵ�᤹�� */
  if (!for_each_segment(ac, cr, cr->res_str, trim_segment)) {
    return ;
  }
  /**/
  if (anthy_get_stat(ac, &acs)) {
    return ;
  }
  cr->cand_check = malloc(sizeof(int) * acs.nr_segment);
  for (i = 0; i < acs.nr_segment; i++) {
    cr->cand_check[i] = 0;
  }

  /* ��������򤹤� */
  if (cr->cand_str) {
    for_each_segment(ac, cr, cr->cand_str, find_candidate);
  }

  if (verbose) {
    anthy_print_context(ac);
  }
  /* ���Ϥ��� */
  print_context_info(ac, cr);
}

int
main(int argc, char **argv)
{
  struct res_db *db;
  struct conv_res *cr;
  anthy_context_t ac;
  int i;

  db = create_db();
  for (i = 1; i < argc; i++) {
    if (!strcmp("-v", argv[i])) {
      verbose = 1;
    } else {
      read_db(db, argv[i]);
    }
  }

  anthy_conf_override("CONFFILE", "../anthy-conf");
  anthy_conf_override("DIC_FILE", "../mkanthydic/anthy.dic");
  anthy_init();
  anthy_set_personality("");
  ac = anthy_create_context();

  /**/
  for (cr = db->res_list.next; cr; cr = cr->next) {
    proc_sentence(ac, cr);
  }
  return 0;
}
