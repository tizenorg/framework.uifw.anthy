/* ���ϤΥ��åȤ�������륳����
 *
 * Copyright (C) 2006 HANAOKA Toshiyuki
 * Copyright (C) 2006-2007 TABATA Yusuke
 *
 * Special Thanks: Google Summer of Code Program 2006
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "input_set.h"

#define HASH_SIZE 1024

struct int_map_node {
  int key;
  int val;
  struct int_map_node *next;
};

struct int_map {
  /**/
  int nr;
  struct int_map_node *hash_head;
  /**/
  int array_size;
  struct int_map_node **array;
};

struct input_set {
  /**/
  struct input_line *lines;
  struct input_line *buckets[HASH_SIZE];
  /**/
  struct int_map *feature_freq;
};

static int
line_hash(const int *ar, int nr)
{
  int i;
  unsigned h = 0;
  for (i = 0; i < nr; i++) {
    h += ar[i];
  }
  return (h % HASH_SIZE);
}

static struct input_line *
find_same_line(struct input_set *is, int *features, int nr)
{
  struct input_line *il;
  int h = line_hash(features, nr);
  for (il = is->buckets[h]; il; il = il->next_in_hash) {
    int i;
    if (il->nr_features != nr) {
      continue;
    }
    for (i = 0; i < nr; i++) {
      if (il->features[i] != features[i]) {
	break;
      }
    }
    if (i >= nr) {
      return il;
    }
  }
  return NULL;
}

static struct input_line *
add_line(struct input_set *is, int *features, int nr)
{
  int i, h;
  struct input_line *il;
  il = malloc(sizeof(struct input_line));
  il->nr_features = nr;
  il->features = malloc(sizeof(int) * nr);
  for (i = 0; i < nr; i++) {
    il->features[i] = features[i];
  }
  il->weight = 0;
  il->negative_weight = 0;
  /* link */
  il->next_line = is->lines;
  is->lines = il;
  /**/
  h = line_hash(features, nr);
  il->next_in_hash = is->buckets[h];
  is->buckets[h] = il;
  return il;
}

static void
add_feature_count(struct int_map *im, int nr, int *features, int weight)
{
  int i;
  for (i = 0; i < nr; i++) {
    int f = features[i];
    int v = int_map_peek(im, f);
    int_map_set(im, f, v + weight);
  }
}

/* input_set�����Ϥ��Ĳä��� */
void
input_set_set_features(struct input_set *is, int *features,
		       int nr, int weight)
{
  struct input_line *il;
  int abs_weight = abs(weight);

  /**/
  il = find_same_line(is, features, nr);
  if (!il) {
    il = add_line(is, features, nr);
  }
  /**/
  if (weight > 0) {
    il->weight += weight;
  } else {
    il->negative_weight += abs_weight;
  }
  /**/
  add_feature_count(is->feature_freq, nr, features, abs_weight);
}

struct input_set *
input_set_create(void)
{
  int i;
  struct input_set *is;
  is = malloc(sizeof(struct input_set));
  is->lines = NULL;
  /**/
  for (i = 0; i < HASH_SIZE; i++) {
    is->buckets[i] = NULL;
  }
  /**/
  is->feature_freq = int_map_new();
  /**/
  return is;
}

struct input_line *
input_set_get_input_line(struct input_set *is)
{
  return is->lines;
}

struct input_set *
input_set_filter(struct input_set *is,
		 double pos, double neg)
{
  struct input_set *new_is = input_set_create();
  struct input_line *il;
  for (il = is->lines; il; il = il->next_line) {
    if (il->weight > pos ||
	il->negative_weight > neg) {
      input_set_set_features(new_is, il->features,
			     il->nr_features, il->weight);
      input_set_set_features(new_is, il->features,
			     il->nr_features, -il->negative_weight);
    }
  }
  return new_is;
}

void
input_set_output_feature_freq(FILE *fp, struct input_set *is)
{
  int i;
  struct int_map *im = is->feature_freq;
  fprintf(fp, "0 %d\n", im->array_size);
  int_map_flatten(im);
  for (i = 0; i < im->array_size; i++) {
    if (!im->array[i]) {
      fprintf(fp, "0 0\n");
    } else {
      fprintf(fp, "%d %d\n", im->array[i]->key,
	      im->array[i]->val);
    }
  }
}

struct int_map *
int_map_new(void)
{
  int i;
  struct int_map *im = malloc(sizeof(struct int_map));
  im->nr = 0;
  im->hash_head = malloc(sizeof(struct int_map_node) * HASH_SIZE);
  for (i = 0; i < HASH_SIZE; i++) {
    im->hash_head[i].next = NULL;
  }
  return im;
}

static int
node_index(int idx)
{
  return idx % HASH_SIZE;
}

static struct int_map_node *
find_int_map_node(struct int_map *im, int idx)
{
  struct int_map_node *node;
  int h = node_index(idx);
  for (node = im->hash_head[h].next; node; node = node->next) {
    if (node->key == idx) {
      return node;
    }
  }
  return NULL;
}

int
int_map_peek(struct int_map *im, int idx)
{
  struct int_map_node *node = find_int_map_node(im, idx);
  if (node) {
    return node->val;
  }
  return 0;
}

void
int_map_set(struct int_map *im, int idx, int val)
{
  struct int_map_node *node = find_int_map_node(im, idx);
  int h;
  if (node) {
    node->val = val;
    return ;
  }
  /**/
  node = malloc(sizeof(struct int_map_node));
  node->key = idx;
  node->val = val;
  h = node_index(idx);
  node->next = im->hash_head[h].next;
  im->hash_head[h].next = node;
  /**/
  im->nr ++;
}

void
int_map_flatten(struct int_map *im)
{
  int i;
  struct int_map_node *node;
  int max_n = 0;
  /* ������������ */
  im->array_size = im->nr * 2;
  im->array = malloc(sizeof(struct int_map_node *) * 
		     im->array_size);
  for (i = 0; i < im->array_size; i++) {
    im->array[i] = NULL;
  }
  /* ���Ǥ��֤��Ƥ��� */
  for (i = 0; i < HASH_SIZE; i++) {
    for (node = im->hash_head[i].next; node; node = node->next) {
      int n = 0;
      while (1) {
	int h;
	h = node->key + n;
	h %= im->array_size;
	if (!im->array[h]) {
	  im->array[h] = node;
	  break;
	}
	/**/
	n++;
      }
      if (n > max_n) {
	max_n = n;
      }
    }
  }
  /**/
  printf(" max_collision=%d\n", max_n);
}
