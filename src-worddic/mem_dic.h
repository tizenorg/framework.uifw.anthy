#ifndef _mem_dic_h_included_
#define _mem_dic_h_included_

#include <anthy/alloc.h>
#include "dic_ent.h"


#define HASH_SIZE 64 /*�ϥå���ơ��֥�Υ�������64(�ʤ�Ȥʤ�)*/

/** ���꼭�� */
struct mem_dic {
  struct seq_ent *seq_ent_hash[HASH_SIZE];
  allocator seq_ent_allocator;
  allocator dic_ent_allocator;
};

#endif
