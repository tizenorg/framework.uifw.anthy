/*
 * �Ŀͼ�������Ѥδؿ���
 *
 * �ߴ������Թ��
 *  utf8�μ����textdict
 *  eucjp�μ����texttrie
 *  �����record��ȤäƤƺ��𤷤ޤ���
 * textdict�ذܹԤ���
 *
 * ��ȯͽ��
 *
 *  ������Ͽ��textdict���Ф��ƹԤ��褦�ˤ��� <- todo
 *  texttrie��ñ��ϰܹԤ���褦�ˤ���
 *  record�ط��Ͼä�
 *
 *
 * Funded by IPA̤Ƨ���եȥ�������¤���� 2001 10/24
 *
 * Copyright (C) 2001-2007 TABATA Yusuke
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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <anthy/anthy.h>
#include <anthy/conf.h>
#include <anthy/dic.h>
#include <anthy/texttrie.h>
#include <anthy/textdict.h>
#include <anthy/dicutil.h>

#include "dic_main.h"
#include "dic_personality.h"

/*
 * �Ŀͼ����texttrie��˳�Ǽ�����Ȥ�
 * ��  ���Ф� ������ -> ��#�ʻ�*���� ñ��פȤ���������Ȥ�
 * (UTF8�ξ��ϡ� p���Ф� ������ -> ��#�ʻ�*���� ñ���)
 * �ǽ��2ʸ���ζ����ñ�����Υ��������Ǥ��뤳�Ȥ��̣����
 * ��������ʬ��Ʊ�������̤��뤿����Ѥ����롣
 *
 */

/* UTF8��32ʸ�� x 3bytes */
#define MAX_KEY_LEN 96

static int gIsInit;
static int dic_util_encoding;

extern struct text_trie *anthy_private_tt_dic;
extern struct textdict *anthy_private_text_dic;
/* �������򤵤�Ƥ����ɤ� */
static struct iterate_contex {
  /**/
  int in_tt;
  /* texttrie */
  char key_buf[MAX_KEY_LEN+32];
  /* textdict�θ����� */
  int dicfile_offset;
  char *current_index;
  char *current_line;
} word_iterator;
/**/
struct scan_context {
  const char *yomi;
  const char *word;
  const char *wt_name;
  int offset;
  int found_word;
};

static void
set_current_line(const char *index, const char *line)
{
  if (word_iterator.current_line) {
    free(word_iterator.current_line);
    word_iterator.current_line = NULL;
  }
  if (line) {
    word_iterator.current_line = strdup(line);
  }
  if (word_iterator.current_index) {
    free(word_iterator.current_index);
    word_iterator.current_index = NULL;
  }
  if (index) {
    word_iterator.current_index = strdup(index);
  }
}

/** �Ŀͼ���饤�֥����������� */
void
anthy_dic_util_init(void)
{
  if (gIsInit) {
    return ;
  }
  if (anthy_init_dic() == -1) {
    return ;
  }
  anthy_dic_set_personality("default");
  gIsInit = 1;
  dic_util_encoding = ANTHY_EUC_JP_ENCODING;
  /**/
  word_iterator.key_buf[0] = 0;
  word_iterator.in_tt = 1;
}

/** ����饤�֥���������� */
void
anthy_dic_util_quit(void)
{
  if (gIsInit) {
    anthy_quit_dic();
  }
  set_current_line(NULL, NULL);
  gIsInit = 0;
}

/** ����桼�ƥ���ƥ�API�Υ��󥳡��ǥ��󥰤����ꤹ�� */
int
anthy_dic_util_set_encoding(int enc)
{
  if (enc == ANTHY_UTF8_ENCODING ||
      enc == ANTHY_EUC_JP_ENCODING) {
    dic_util_encoding = enc;
  }
  return dic_util_encoding;
}

void
anthy_dic_util_set_personality(const char *id)
{
  anthy_dic_set_personality(id);
}

static char *
find_next_key(const char *prefix)
{
  char *v;
  v = anthy_trie_find_next_key(anthy_private_tt_dic,
			       word_iterator.key_buf, MAX_KEY_LEN+32);

  if (v && v[0] == prefix[0] && v[1] == prefix[1]) {
    /* ����key����ꤵ�줿prefix����äƤ��� */
    return v;
  }
  /**/
  sprintf(word_iterator.key_buf, "%s", prefix);
  return NULL;
}

static void
delete_prefix(const char *prefix)
{
  sprintf(word_iterator.key_buf, "%s", prefix);
  anthy_priv_dic_lock();
  /* word_iterator.key_buf��prefix��ʸ����Ǥ���С�find_next_key()��
     �ǽ��ñ����֤� */
  while (find_next_key(prefix)) {
    anthy_trie_delete(anthy_private_tt_dic, word_iterator.key_buf);
    sprintf(word_iterator.key_buf, "%s", prefix);
  }
  anthy_priv_dic_unlock();
}

static const char *
encoding_prefix(int encoding)
{
  if (encoding == ANTHY_UTF8_ENCODING) {
    return " p";
  }
  /* EUC-JP */
  return "  ";
}

/** (API) �Ŀͼ���������ä� */
void
anthy_priv_dic_delete(void)
{
  delete_prefix(encoding_prefix(ANTHY_EUC_JP_ENCODING));
  /**/
  while (!anthy_textdict_delete_line(anthy_private_text_dic, 0)) {
    /**/
  }
}

static int
scan_one_word_cb(void *p, int next_offset, const char *key, const char *n)
{
  (void)p;
  set_current_line(key, n);
  word_iterator.dicfile_offset = next_offset;
  return -1;
}

static int
select_first_entry_in_textdict(void)
{
  word_iterator.dicfile_offset = 0;
  set_current_line(NULL, NULL);
  anthy_textdict_scan(anthy_private_text_dic,
		      word_iterator.dicfile_offset, NULL,
		      scan_one_word_cb);
  if (word_iterator.current_line) {
    word_iterator.in_tt = 0;
    return 0;
  }
  /* ñ�줬̵�� */
  return ANTHY_DIC_UTIL_ERROR;
}

/** (API) �ǽ��ñ������򤹤� */
int
anthy_priv_dic_select_first_entry(void)
{
  if (dic_util_encoding == ANTHY_UTF8_ENCODING) {
    return select_first_entry_in_textdict();
  }
  if (anthy_private_tt_dic) {
    sprintf(word_iterator.key_buf, "%s", encoding_prefix(dic_util_encoding));
    /* prefix�μ��Υ���ȥ꤬�ǽ�Υ���ȥ� */
    if (find_next_key(encoding_prefix(dic_util_encoding))) {
      word_iterator.in_tt = 1;
      return 0;
    }
  }
  /* ñ�줬̵���Τ�textdict�˰�ư���ߤ� */
  return select_first_entry_in_textdict();
}

/** (API) �������򤵤�Ƥ���ñ��μ���ñ������򤹤� */
int
anthy_priv_dic_select_next_entry(void)
{
  if (!word_iterator.in_tt) {
    set_current_line(NULL, NULL);
    anthy_textdict_scan(anthy_private_text_dic, word_iterator.dicfile_offset,
			NULL,
			scan_one_word_cb);
    if (word_iterator.current_line) {
      return 0;
    }
    return ANTHY_DIC_UTIL_ERROR;
  }
  if (find_next_key(encoding_prefix(dic_util_encoding))) {
    return 0;
  }
  /* ñ�줬̵���Τ�textdict�˰�ư���ߤ� */
  return select_first_entry_in_textdict();
}

/** ̤���� */
int
anthy_priv_dic_select_entry(const char *index)
{
  (void)index;
  return 0;
}

/** �������򤵤�Ƥ���ñ����ɤߤ��������� */
char *
anthy_priv_dic_get_index(char *buf, int len)
{
  int i;
  char *src_buf;
  if (word_iterator.in_tt) {
    src_buf = &word_iterator.key_buf[2];
  } else {
    src_buf = word_iterator.current_index;
  }
  if (!word_iterator.in_tt && dic_util_encoding == ANTHY_EUC_JP_ENCODING) {
    /**/
    src_buf = anthy_conv_utf8_to_euc(src_buf);
  } else {
    src_buf = strdup(src_buf);
  }
  /* �ǽ�ζ���\0�ޤǤ򥳥ԡ����� */
  for (i = 0; src_buf[i] && src_buf[i] != ' '; i++) {
    if (i >= len - 1) {
      free(src_buf);
      return NULL;
    }
    buf[i] = src_buf[i];
  }
  buf[i] = 0;
  free(src_buf);
  return buf;
}

/** �������򤵤�Ƥ���ñ������٤�������� */
int
anthy_priv_dic_get_freq(void)
{
  struct word_line res;
  char *v;
  if (word_iterator.in_tt) {
    v = anthy_trie_find(anthy_private_tt_dic, word_iterator.key_buf);
    anthy_parse_word_line(v, &res);
    free(v);
  } else {
    anthy_parse_word_line(word_iterator.current_line, &res);
  }
  return res.freq;
}

/** �������򤵤�Ƥ���ñ����ʻ��������� */
char *
anthy_priv_dic_get_wtype(char *buf, int len)
{
  struct word_line res;
  char *v;
  if (word_iterator.in_tt) {
    v = anthy_trie_find(anthy_private_tt_dic, word_iterator.key_buf);
    anthy_parse_word_line(v, &res);
    free(v);
  } else {
    anthy_parse_word_line(word_iterator.current_line, &res);
  }
  if (len - 1 < (int)strlen(res.wt)) {
    return NULL;
  }
  sprintf(buf, "%s", res.wt);
  return buf;
}

/** �������򤵤�Ƥ���ñ���������� */
char *
anthy_priv_dic_get_word(char *buf, int len)
{
  char *v;
  char *s;
  if (word_iterator.in_tt) {
    v = anthy_trie_find(anthy_private_tt_dic, word_iterator.key_buf);
  } else {
    v = word_iterator.current_line;
  }
  if (!v) {
    return NULL;
  }
  /* �ʻ�θ��ˤ���ñ�����Ф� */
  s = strchr(v, ' ');
  s++;
  if (!word_iterator.in_tt && dic_util_encoding == ANTHY_EUC_JP_ENCODING) {
    s = anthy_conv_utf8_to_euc(s);
    snprintf(buf, len, "%s", s);
    free(s);
  } else {
    snprintf(buf, len, "%s", s);
  }
  if (word_iterator.in_tt) {
    free(v);
  }
  return buf;
}

static int
find_cb(void *p, int next_offset, const char *key, const char *n)
{
  struct scan_context *sc = p;
  struct word_line res;
  if (strcmp(key, sc->yomi)) {
    sc->offset = next_offset;
    return 0;
  }
  anthy_parse_word_line(n, &res);
  if (!strcmp(res.wt, sc->wt_name) &&
      !strcmp(res.word, sc->word)) {
    sc->found_word = 1;
    return -1;
  }
  sc->offset = next_offset;
  return 0;
}

static int
order_cb(void *p, int next_offset, const char *key, const char *n)
{
  struct scan_context *sc = p;
  (void)n;
  if (strcmp(key, sc->yomi) >= 0) {
    sc->found_word = 1;
    return -1;
  }
  sc->offset = next_offset;
  return 0;
}

/* ������utf8 */
static int
do_add_word_to_textdict(struct textdict *td, int offset,
			const char *yomi, const char *word,
			const char *wt_name, int freq)
{
  char *buf = malloc(strlen(yomi) + strlen(word) + strlen(wt_name) + 20);
  int rv;
  if (!buf) {
    return -1;
  }
  sprintf(buf, "%s %s*%d %s\n", yomi, wt_name, freq, word);
  rv = anthy_textdict_insert_line(td, offset, buf);
  free(buf);
  return rv;
}

static int
dup_word_check(const char *v, const char *word, const char *wt)
{
  struct word_line res;

  if (anthy_parse_word_line(v, &res)) {
    return 0;
  }

  /* �ɤߤ�ñ�����Ӥ��� */
  if (!strcmp(res.wt, wt) &&
      !strcmp(res.word, word)) {
    return 1;
  }
  return 0;
}

static int
find_same_word(char *idx_buf, const char *yomi,
	       const char *word, const char *wt_name, int yomi_len)
{
  int found = 0;
  sprintf(idx_buf, "%s%s ",
	  encoding_prefix(dic_util_encoding),
	  yomi);
  anthy_trie_find_next_key(anthy_private_tt_dic,
			   idx_buf, yomi_len + 12);

  /* trie�Υ���ǥå�����õ�� */
  do {
    char *v;
    if (strncmp(&idx_buf[2], yomi, yomi_len) ||
	idx_buf[yomi_len+2] != ' ') {
      /* ���и줬�ۤʤ�Τǥ롼�׽�λ */
      break;
    }
    /* texttrie�˥����������ơ����и�ʳ�����פ��Ƥ��뤫������å� */
    v = anthy_trie_find(anthy_private_tt_dic, idx_buf);
    if (v) {
      found = dup_word_check(v, word, wt_name);
      free(v);
      if (found) {
	break;
      }
    }
  } while (anthy_trie_find_next_key(anthy_private_tt_dic,
				    idx_buf, yomi_len + 12));

  return found;
}

static int
add_word_to_textdict(const char *yomi, const char *word,
		     const char *wt_name, int freq)
{
  struct scan_context sc;
  int rv;
  int yomi_len = strlen(yomi);

  if (yomi_len > MAX_KEY_LEN || yomi_len == 0) {
    return ANTHY_DIC_UTIL_ERROR;
  }

  if (wt_name[0] != '#') {
    return ANTHY_DIC_UTIL_ERROR;
  }

  /* texttrie�ˤ���оä� */
  if (anthy_private_tt_dic) {
    char *idx_buf = malloc(yomi_len + 12);
    if (find_same_word(idx_buf, yomi, word, wt_name, yomi_len)) {
      anthy_trie_delete(anthy_private_tt_dic, idx_buf);
    }
    free(idx_buf);
  }

  /* Ʊ��ʪ�����ä���ä� */
  sc.yomi = yomi;
  sc.word = word;
  sc.wt_name = wt_name;
  /**/
  sc.offset = 0;
  sc.found_word = 0;
  anthy_textdict_scan(anthy_private_text_dic, 0, &sc,
		      find_cb);
  if (sc.found_word == 1) {
    anthy_textdict_delete_line(anthy_private_text_dic, sc.offset);
  }
  if (freq == 0) {
    return ANTHY_DIC_UTIL_OK;
  }
  /* �ɲä������õ�� */
  sc.offset = 0;
  sc.found_word = 0;
  anthy_textdict_scan(anthy_private_text_dic, 0, &sc,
		      order_cb);
  /* �ɲä��� */
  rv = do_add_word_to_textdict(anthy_private_text_dic, sc.offset,
			       yomi, word, wt_name, freq);
  if (!rv) {
    return ANTHY_DIC_UTIL_OK;
  }
  return ANTHY_DIC_UTIL_ERROR;
}

/** ñ�����Ͽ����
 * ���٤�0�ξ��Ϻ��
 */
int
anthy_priv_dic_add_entry(const char *yomi, const char *word,
			 const char *wt_name, int freq)
{
  if (dic_util_encoding == ANTHY_UTF8_ENCODING) {
    return add_word_to_textdict(yomi, word, wt_name, freq);
  } else {
    int rv;
    char *yomi_utf8 = anthy_conv_euc_to_utf8(yomi);
    char *word_utf8 = anthy_conv_euc_to_utf8(word);
    rv = add_word_to_textdict(yomi_utf8, word_utf8, wt_name, freq);
    free(yomi_utf8);
    free(word_utf8);
    return rv;
  }
}

const char *
anthy_dic_util_get_anthydir(void)
{
  return anthy_conf_get_str("ANTHYDIR");
}

/* look���ޥ�ɤμ���򸡺����뤿��δؿ� */
static char *
do_search(FILE *fp, const char *word)
{
  char buf[32];
  char *res = NULL;
  int word_len = strlen(word);
  while (fgets(buf, 32, fp)) {
    int len = strlen(buf);
    buf[len - 1] = 0;
    len --;
    if (len > word_len) {
      continue;
    }
    if (!strncasecmp(buf, word, len)) {
      if (res) {
	free(res);
      }
      res = strdup(buf);
    }
  }
  return res;
}

/* look���ޥ�ɤμ���򸡺�����API */
char *
anthy_dic_search_words_file(const char *word)
{
  FILE *fp;
  char *res;
  const char *words_dict_fn = anthy_conf_get_str("WORDS_FILE");
  if (!words_dict_fn) {
    return NULL;
  }
  fp = fopen(words_dict_fn, "r");
  if (!fp) {
    return NULL;
  }
  res = do_search(fp, word);
  fclose(fp);
  return res;
}
