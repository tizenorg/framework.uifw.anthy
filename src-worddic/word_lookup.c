/*
 * Word Dictionary
 * �ե�����μ���Υ��󥿡��ե�������¸�ߤ���ǡ�����
 * ����å��夵���ΤǤ����Ǥ�¸�ߤ��ʤ�ñ���
 * ���������®�ˤ���ɬ�פ����롣
 *
 * anthy_gang_fill_seq_ent()���濴�Ȥʤ�ؿ��Ǥ���
 *  ���ꤷ��word_dic������ꤷ��ʸ����򥤥�ǥå����Ȥ��Ƥ�ĥ���ȥ��
 *  �������ղä���seq_ent���ɲä���
 *
 * a)����η�����b)���񥢥������ι�®��c)����ե�����Υ��󥳡��ǥ���
 *  ���Υ�������ǰ��äƤ�ΤǤ��ʤ�ʣ�������Ƥޤ���
 *
 * Copyright (C) 2000-2007 TABATA Yusuke
 * Copyright (C) 2005-2006 YOSHIDA Yuichi
 * Copyright (C) 2001-2002 TAKAI Kosuke
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
#include <ctype.h>

#include "config.h"
#include <anthy/anthy.h>
#include <anthy/alloc.h>
#include <anthy/dic.h>
#include <anthy/word_dic.h>
#include <anthy/logger.h>
#include <anthy/xstr.h>
#include <anthy/diclib.h>

#include "dic_main.h"
#include "dic_ent.h"

#define NO_WORD -1

static allocator word_dic_ator;

struct lookup_context {
  struct gang_elm **array;
  int nr;
  int nth;
  int is_reverse;
};

/* 1�Х����ܤ򸫤ơ�ʸ�������Х��Ȥ��뤫���֤� */
static int
mb_fragment_len(const char *str)
{
  unsigned char c = *((const unsigned char *)str);
  if (c < 0x80) {
    return 1;
  }
  if (c < 0xe0) {
    return 2;
  }
  if (c < 0xf0) {
    return 3;
  }
  if (c < 0xf8) {
    return 4;
  }
  if (c < 0xfc) {
    return 5;
  }
  return 6;
}

static int
is_printable(char *str)
{
  unsigned char *tmp = (unsigned char *)str;
  if (*tmp > 31 && *tmp < 127) {
    return 1;
  }
  if (mb_fragment_len(str) > 1) {
    return 1;
  }
  return 0;
}

/* ����Υ��󥳡��ǥ��󥰤���xchar���� */
static xchar
form_mb_char(const char *str)
{
  xchar xc;
  anthy_utf8_to_ucs4_xchar(str, &xc);
  return xc;
}

static int
hash(xstr *x)
{
  return anthy_xstr_hash(x)&
    (YOMI_HASH_ARRAY_SIZE*YOMI_HASH_ARRAY_BITS-1);
}

static int
check_hash_ent(struct word_dic *wdic, xstr *xs)
{
  int val = hash(xs);
  int idx = (val>>YOMI_HASH_ARRAY_SHIFT)&(YOMI_HASH_ARRAY_SIZE-1);
  int bit = val & ((1<<YOMI_HASH_ARRAY_SHIFT)-1);
  return wdic->hash_ent[idx] & (1<<bit);
}

static int
wtype_str_len(const char *str)
{
  int i;
  for (i = 0; str[i] && str[i]!= ' '; i++);
  return i;
}

/* ����ι���򥹥���󤹤뤿��ξ����ݻ� */
struct wt_stat {
  wtype_t wt;
  const char *wt_name;
  int feature;
  int freq;
  int order_bonus;/* ������ν���ˤ�����٤Υܡ��ʥ� */
  int offset;/* ʸ������Υ��ե��å� */
  const char *line;
  int encoding;
};
/*
 * #XX*123 �Ȥ���Cannadic�η�����ѡ�������
 *  #XX
 *  #XX*123
 *  #XX,x*123
 */
static const char *
parse_wtype_str(struct wt_stat *ws)
{
  int len;
  char *buf;
  char *freq_part;
  char *feature_part;
  const char *wt_name;
  /* �Хåե��إ��ԡ����� */
  len = wtype_str_len(&ws->line[ws->offset]);
  buf = alloca(len + 1);
  strncpy(buf, &ws->line[ws->offset], len);
  buf[len] = 0;

  /* ����(̤����) */
  feature_part = strchr(buf, ',');
  if (feature_part) {
    ws->feature = 1;
  } else {
    ws->feature = 0;
  }

  /* ���٤�parse���� */
  freq_part = strchr(buf, '*');
  if (freq_part) {
    *freq_part = 0;
    freq_part ++;
    ws->freq = atoi(freq_part) * FREQ_RATIO;
  } else {
    ws->freq = FREQ_RATIO - 2;
  }

  /**/
  wt_name = anthy_type_to_wtype(buf, &ws->wt);
  if (!wt_name) {
    ws->wt = anthy_wt_none;
  }
  ws->offset += len;
  return wt_name;
}


static int
normalize_freq(struct wt_stat* ws)
{
  if (ws->freq < 0) {
    ws->freq *= -1;
  }
  return ws->freq + ws->order_bonus;
}

/* '\\'�ˤ�륨�������פ��б��������ԡ� */
static void
copy_to_buf(char *buf, const char *src, int char_count)
{
  int pos;
  int i;
  pos = 0;
  for (i = 0; i < char_count; i++){
    if (src[i] == '\\') {
      if (src[i + 1] == ' ') {
	i ++;
      } else if (src[i + 1] == '\\') {
	i ++;
      }
    }
    buf[pos] = src[i];
    pos ++;
  }
  buf[pos] = 0;
}

/** seq_ent��dic_ent���ɲä��� */
static int
add_dic_ent(struct seq_ent *seq, struct wt_stat *ws,
	    xstr* yomi, int is_reverse)
{
  int i;
  /* ����ե�������ΥХ��ȿ� */
  int char_count;
  char *buf;
  xstr *xs;
  int freq;
  wtype_t w = ws->wt;
  const char *s = &ws->line[ws->offset];

  /* ñ���ʸ������׻� */
  for (i = 0, char_count = 0;
       s[i] && (s[i] != ' ') && (s[i] != '#'); i++) {
    char_count ++;
    if (s[i] == '\\') {
      char_count++;
      i++;
    }
  }

  /* �ʻ줬�������Ƥ��ʤ��Τ�̵�� */
  if (!ws->wt_name) {
    return char_count;
  }

  /* freq����ʤΤϵ��Ѵ��� */
  if (!is_reverse && ws->freq < 0) {
    return char_count;
  }

  /* buf��ñ��򥳥ԡ� */
  buf = alloca(char_count+1);
  copy_to_buf(buf, s, char_count);

  xs = anthy_cstr_to_xstr(buf, ws->encoding);

  /* freq�����ʤΤϽ��Ѵ��� */
  if (is_reverse && ws->freq > 0) {
    /* ���Ѵ��κݤˡ��Ѵ��Ѥߤ���ʬ��̤�Ѵ�����ʬ�������äƤ��������б�����٤ˡ�
       ʿ��̾�Τߤ���ʤ���ʬ�Ͻ缭��ˤ����ɤߤ����ñ�줬�����dic_ent���������롣
    */
    if (anthy_get_xstr_type(yomi) & XCT_HIRA) {
      freq = normalize_freq(ws);
      anthy_mem_dic_push_back_dic_ent(seq, 0, yomi, w,
				      ws->wt_name, freq, 0);
    }      
    anthy_free_xstr(xs);
    return char_count;
  }

  freq = normalize_freq(ws);

  anthy_mem_dic_push_back_dic_ent(seq, 0, xs, w, ws->wt_name, freq, 0);
  if (anthy_wtype_get_meisi(w)) {
    /* Ϣ�ѷ���̾�첽�����Ĥ�̾�첽������Τ��ɲ� */
    w = anthy_get_wtype_with_ct(w, CT_MEISIKA);
    anthy_mem_dic_push_back_dic_ent(seq, 0, xs, w, ws->wt_name, freq, 0);
  }
  anthy_free_xstr(xs);
  return char_count;
}

static int
add_compound_ent(struct seq_ent *seq, struct wt_stat *ws,
		 xstr* yomi,
		 int is_reverse)
{
  int len = wtype_str_len(&ws->line[ws->offset]);
  char *buf = alloca(len);
  xstr *xs;
  int freq;

  (void)yomi;

  /* freq����ʤΤϵ��Ѵ��� */
  if (!is_reverse && ws->freq < 0) {
    /* ���ʤ��Ѵ��Ǥ��פ�ʤ� */
    return len;
  }

  /* freq�����ʤΤϽ��Ѵ��� */
  if (is_reverse && ws->freq > 0) {

    /* ���Ѵ��κݤˡ��Ѵ��Ѥߤ���ʬ��̤�Ѵ�����ʬ�������äƤ��������б�����٤ˡ�
       ʿ��̾�Τߤ���ʤ���ʬ�Ͻ缭��ˤ����ɤߤ����ñ�줬�����dic_ent���������롣
    */
    /*
      yomi��#_�����ղä���ʸ�������ɬ�פ�����
    if (anthy_get_xstr_type(yomi) & (XCT_HIRA | XCT_KATA)) {
      freq = normalize_freq(ws);
      anthy_mem_dic_push_back_compound_ent(seq, xs, ws->wt, freq);
    }
    */
    return len;
  }

  strncpy(buf, &ws->line[ws->offset + 1], len - 1);
  buf[len - 1] = 0;
  xs = anthy_cstr_to_xstr(buf, ws->encoding);

  freq = normalize_freq(ws);
  anthy_mem_dic_push_back_dic_ent(seq, 1, xs, ws->wt,
				  ws->wt_name, freq, 0);
  anthy_free_xstr(xs);

  return len;
}

static void
init_wt_stat(struct wt_stat *ws, char *line)
{
  ws->wt_name = NULL;
  ws->freq = 0;
  ws->feature = 0;
  ws->order_bonus = 0;
  ws->offset = 0;
  ws->line = line;
  ws->encoding = ANTHY_EUC_JP_ENCODING;
  if (*(ws->line) == 'u') {
    ws->encoding = ANTHY_UTF8_ENCODING;
    ws->line ++;
  }
}

/** ����Υ���ȥ�ξ���򸵤�seq_ent�򤦤�� */
static void
fill_dic_ent(char *line, struct seq_ent *seq, 
	     xstr* yomi, int is_reverse)
{
  struct wt_stat ws;
  init_wt_stat(&ws, line);

  while (ws.line[ws.offset]) {
    if (ws.line[ws.offset] == '#') {
      if (isalpha(ws.line[ws.offset + 1])) {
	/* �ʻ�*���� */
	ws.wt_name = parse_wtype_str(&ws);
	/**/
	ws.order_bonus = FREQ_RATIO - 1;
      } else {
	/* ʣ������ */
	ws.offset += add_compound_ent(seq, &ws,
				      yomi,
				      is_reverse);
      }
    } else {
      /* ñ�� */
      ws.offset += add_dic_ent(seq, &ws, yomi,
			       is_reverse);
      if (ws.order_bonus > 0) {
	ws.order_bonus --;
      }
    }
    if (ws.line[ws.offset] == ' ') {
      ws.offset++;
    }
  }
}

/*
 * s�˽񤫤줿ʸ����ˤ�ä�x���ѹ�����
 * �֤��ͤ��ɤ߿ʤ᤿�Х��ȿ�
 */
static int
mkxstr(char *s, xstr *x)
{
  int i, len;
  /* s[0]�ˤϴ����ᤷ��ʸ���� */
  x->len -= (s[0] - 1);
  for (i = 1; is_printable(&s[i]); i ++) {
    len = mb_fragment_len(&s[i]);
    if (len > 1) {
      /* �ޥ���Х��� */
      x->str[x->len] = form_mb_char(&s[i]);
      x->len ++;
      i += (len - 1);
    } else {
      /* 1�Х���ʸ�� */
      x->str[x->len] = s[i];
      x->len ++;
    }
  } 
  return i;
}

static int
set_next_idx(struct lookup_context *lc)
{
  lc->nth ++;
  while (lc->nth < lc->nr) {
    if (lc->array[lc->nth]->tmp.idx != NO_WORD) {
      return 1;
    }
    lc->nth ++;
  }
  return 0;
}

/** �ڡ������ñ��ξ���Ĵ�٤� */
static void
search_words_in_page(struct lookup_context *lc, int page, char *s)
{
  int o = 0;
  xchar *buf;
  xstr xs;
  int nr = 0;
  /* ���Υڡ�����ˤ����äȤ�Ĺ��ñ����Ǽ������Ĺ�� */
  buf = alloca(sizeof(xchar)*strlen(s)/2);
  xs.str = buf;
  xs.len = 0;

  while (*s) {
    int r;
    s += mkxstr(s, &xs);
    r = anthy_xstrcmp(&xs, &lc->array[lc->nth]->xs);
    if (!r) {
      lc->array[lc->nth]->tmp.idx = o + page * WORDS_PER_PAGE;
      nr ++;
      if (!set_next_idx(lc)) {
	return ;
      }
      /* Ʊ���ڡ�����Ǽ���ñ���õ�� */
    }
    o ++;
  }
  if (nr == 0) {
    /* ���Υڡ�����1��⸫�Ĥ���ʤ��ä��顢����ñ���̵�� */
    lc->array[lc->nth]->tmp.idx = NO_WORD;
    set_next_idx(lc);
  }
  /* ���ߤ�ñ��ϼ��θƤӽФ���õ�� */
}

/**/
static int
compare_page_index(struct word_dic *wdic, const char *key, int page)
{
  char buf[100];
  char *s = &wdic->page[anthy_dic_ntohl(wdic->page_index[page])];
  int i;
  s++;
  for (i = 0; is_printable(&s[i]);) {
    int j, l = mb_fragment_len(&s[i]);
    for (j = 0; j < l; j++) {
      buf[i+j] = s[i+j];
    }
    i += l;
  }
  buf[i] = 0;
  return strcmp(key ,buf);
}

/* �Ƶ�Ū�˥Х��ʥꥵ�����򤹤� */
static int
get_page_index_search(struct word_dic *wdic, const char *key, int f, int t)
{
  /* anthy_xstrcmp��-1��̵���ʤä��Ȥ����õ�� */
  int c,p;
  c = (f+t)/2;
  if (f+1==t) {
    return c;
  } else {
    p = compare_page_index(wdic, key, c);
    if (p < 0) {
      return get_page_index_search(wdic, key, f, c);
    } else {
      /* c<= <t */
      return get_page_index_search(wdic, key, c, t);
    }
  } 
}

/** key��ޤ��ǽ���Τ���ڡ������ֹ�����롢
 * �ϰϥ����å��򤷤ƥХ��ʥꥵ������Ԥ�get_page_index_search��Ƥ�
 */
static int
get_page_index(struct word_dic *wdic, struct lookup_context *lc)
{
  int page;
  const char *key = lc->array[lc->nth]->key;
  /* �ǽ�Υڡ������ɤߤ��⾮���� */
  if (compare_page_index(wdic, key, 0) < 0) {
    return -1;
  }
  /* �Ǹ�Υڡ������ɤߤ����礭���Τǡ��Ǹ�Υڡ����˴ޤޤ���ǽ�������� */
  if (compare_page_index(wdic, key, wdic->nr_pages-1) >= 0) {
    return wdic->nr_pages-1;
  }
  /* �������� */
  page = get_page_index_search(wdic, key, 0, wdic->nr_pages);
  return page;
}

static int
get_nr_page(struct word_dic *h)
{
  int i;
  for (i = 1; anthy_dic_ntohl(h->page_index[i]); i++);
  return i;
}

static char *
get_section(struct word_dic *wdic, int section)
{
  int *p = (int *)wdic->dic_file;
  int offset = anthy_dic_ntohl(p[section]);
  return &wdic->dic_file[offset];
}

/** ����ե������mmap���ơ�word_dic��γƥ��������Υݥ��󥿤�������� */
static int
get_word_dic_sections(struct word_dic *wdic)
{
  wdic->entry_index = (int *)get_section(wdic, 2);
  wdic->entry = (char *)get_section(wdic, 3);
  wdic->page = (char *)get_section(wdic, 4);
  wdic->page_index = (int *)get_section(wdic, 5);
  wdic->uc_section = (char *)get_section(wdic, 6);
  wdic->hash_ent = (unsigned char *)get_section(wdic, 7);

  return 0;
}

/** ���ꤵ�줿ñ��μ�����Υ���ǥå�����Ĵ�٤� */
static void
search_yomi_index(struct word_dic *wdic, struct lookup_context *lc)
{
  int p;
  int page_number;

  /* ���Ǥ�̵�����Ȥ�ʬ���äƤ��� */
  if (lc->array[lc->nth]->tmp.idx == NO_WORD) {
    set_next_idx(lc);
    return ;
  }

  p = get_page_index(wdic, lc);
  if (p == -1) {
    lc->array[lc->nth]->tmp.idx = NO_WORD;
    set_next_idx(lc);
    return ;
  }

  page_number = anthy_dic_ntohl(wdic->page_index[p]);
  search_words_in_page(lc, p, &wdic->page[page_number]);
}

static void
find_words(struct word_dic *wdic, struct lookup_context *lc)
{
  int i;
  /* �������˽��� */
  for (i = 0; i < lc->nr; i++) {
    lc->array[i]->tmp.idx = NO_WORD;
    if (lc->array[i]->xs.len > 31) {
      /* 32ʸ���ʾ�ñ��ˤ�̤�б� */
      continue;
    }
    /* hash�ˤʤ��ʤ���� */
    if (!check_hash_ent(wdic, &lc->array[i]->xs)) {
      continue;
    }
    /* NO_WORD�Ǥʤ��ͤ����ꤹ�뤳�ȤǸ����оݤȤ��� */
    lc->array[i]->tmp.idx = 0;
  }
  /* �������� */
  lc->nth = 0;
  while (lc->nth < lc->nr) {
    search_yomi_index(wdic, lc);
  }
}

static void
load_words(struct word_dic *wdic, struct lookup_context *lc)
{
  int i;
  for (i = 0; i < lc->nr; i++) {
    int yomi_index;
    yomi_index = lc->array[i]->tmp.idx;
    if (yomi_index != NO_WORD) {
      int entry_index;
      struct seq_ent *seq;
      seq = anthy_cache_get_seq_ent(&lc->array[i]->xs,
				    lc->is_reverse);
      entry_index = anthy_dic_ntohl(wdic->entry_index[yomi_index]);
      fill_dic_ent(&wdic->entry[entry_index],
		   seq,
		   &lc->array[i]->xs,
		   lc->is_reverse);
      anthy_validate_seq_ent(seq, &lc->array[i]->xs, lc->is_reverse);
    }
  }
}

/** word_dic����ñ��򸡺�����
 * ���񥭥�å��夫��ƤФ��
 * (gang lookup�ˤ��뤳�Ȥ�Ƥ����)
 */
void
anthy_gang_fill_seq_ent(struct word_dic *wdic,
			struct gang_elm **array, int nr,
			int is_reverse)
{
  struct lookup_context lc;
  lc.array = array;
  lc.nr = nr;
  lc.is_reverse = is_reverse;

  /* ��ñ��ξ���õ�� */
  find_words(wdic, &lc);
  /* ñ��ξ�����ɤ߹��� */
  load_words(wdic, &lc);
}

struct word_dic *
anthy_create_word_dic(void)
{
  struct word_dic *wdic;
  char *p;

  wdic = anthy_smalloc(word_dic_ator);
  memset(wdic, 0, sizeof(*wdic));

  /* ����ե������ޥåפ��� */
  wdic->dic_file = anthy_file_dic_get_section("word_dic");

  /* �ƥ��������Υݥ��󥿤�������� */
  if (get_word_dic_sections(wdic) == -1) {
    anthy_sfree(word_dic_ator, wdic);
    return 0;
  }
  wdic->nr_pages = get_nr_page(wdic);

  /* ���㼭���ޥåפ��� */
  p = wdic->uc_section;
  return wdic;
}

void
anthy_release_word_dic(struct word_dic *wdic)
{
  anthy_sfree(word_dic_ator, wdic);
}

void
anthy_init_word_dic(void)
{
  word_dic_ator = anthy_create_allocator(sizeof(struct word_dic), NULL);
}
