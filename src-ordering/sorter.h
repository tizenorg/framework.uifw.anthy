#ifndef _sorter_h_included_
#define _sorter_h_included_

/* candswap.c */
/* ��̤Ȥ��ƽФ�������ǤϤʤ����䤬���ߥåȤ��줿 */
void anthy_swap_cand_ent(struct cand_ent *old_one, struct cand_ent *new_one);
/**/
void anthy_proc_swap_candidate(struct seg_ent *se);
/* ���ߥåȻ���candswap�ε�Ͽ��aging���� */
void anthy_cand_swap_ageup(void);

/**/
void anthy_reorder_candidates_by_relation(struct segment_list *sl, int nth);

void anthy_learn_cand_history(struct segment_list *sl);
void anthy_reorder_candidates_by_history(struct seg_ent *se);

#endif
