/*
 * ���Ѥ�����ե�����ѡ���
 */
#ifndef _ruleparser_h_included_
#define _ruleparser_h_included_

/*
 * �ե�����̾��'/'�ǻϤޤäƤ�������Хѥ�
 * �ե�����̾��'./'�ǻϤޤäƤ���Х����ȥǥ��쥯�ȥ�
 * �ե�����̾��NULL�ʤ��ɸ������
 * �����Ǥʤ���С�ANTHYDIR��Υե�����򳫤���
 */
int anthy_open_file(const char *fn);/* returns 0 on success */
void anthy_close_file(void);
int anthy_read_line(char ***tokens, int *nr);/* returns 0 on success */
int anthy_get_line_number(void);
void anthy_free_line(void);

#endif
