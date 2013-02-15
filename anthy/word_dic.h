/* ����饤�֥��(libanthydic)��
 * ����������ξ������Ȥ�
 * �ե����뼭��ι�¤
 */
#ifndef _word_dic_h_included_
#define _word_dic_h_included_

/* �ɤ�hash��bit map���礭�� */
#define YOMI_HASH_ARRAY_SIZE (65536*4)
#define YOMI_HASH_ARRAY_SHIFT 3
#define YOMI_HASH_ARRAY_BITS (1<<YOMI_HASH_ARRAY_SHIFT)

/* ���Ѥ�hash */
#define VERSATILE_HASH_SIZE (128*1024)

/* 1�ڡ�����ˤ����Ĥ�ñ�������뤫 */
#define WORDS_PER_PAGE 64

/** ����ե����� 
 * ����饤�֥����
 */
struct word_dic {
  /** ����ե����뼫�ΤΥݥ��� */
  char *dic_file;
  /** ���񥨥�ȥ�Υ���ǥå���������(�ͥåȥ���Х��ȥ�������) */
  int *entry_index;
  /** ���񥨥�ȥ� */
  char *entry;
  /** ����ǥå����ؤΥ���ǥå��� */
  int *page_index;
  /** ����Υ���ǥå��� */
  char *page;
  /** ���㼭�� */
  char *uc_section;

  /* ñ�켭�� */
  int nr_pages;
  unsigned char *hash_ent;
};

#endif

