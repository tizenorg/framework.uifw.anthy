/* �ǥХå��䥨�顼�Υ�å������ν��� */
#ifndef _logger_h_included_
#define _logger_h_included_

void anthy_do_set_logger(void (*)(int , const char*), int lv);
void anthy_log(int lv, const char *, ...);

#endif
