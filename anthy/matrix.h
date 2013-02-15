/* �¹���򰷤�����Υ����� */
#ifndef _matrix_h_included_
#define _matrix_h_included_

struct matrix_image {
  /* number of 'int elements */
  int size;
  /* array of 'int */
  int *image;
};

struct sparse_matrix;
struct sparse_array;

/* �����������API */
struct sparse_matrix *anthy_sparse_matrix_new(void);
void anthy_sparse_matrix_set(struct sparse_matrix *m, int row, int column,
			     int value, void *ptr);
int anthy_sparse_matrix_get_int(struct sparse_matrix *m, int row, int column);
void anthy_sparse_matrix_make_matrix(struct sparse_matrix *m);
/* ���󥤥᡼�����ۤ���API(�����host byte order) */
struct matrix_image *anthy_matrix_image_new(struct sparse_matrix *s);
/* ���󥤥᡼���˥�����������API(�����network byte order) */
int anthy_matrix_image_peek(int *im, int row, int col);

#endif
