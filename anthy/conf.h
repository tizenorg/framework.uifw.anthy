/* $B@_Dj$r<hF@$9$k$?$a$N%$%s%?%U%'!<%9(B */
#ifndef _conf_h_included_
#define _conf_h_included_

void anthy_do_conf_init(void);
void anthy_do_conf_override(const char *, const char *);
void anthy_conf_free(void);

const char *anthy_conf_get_str(const char *var);

#endif
