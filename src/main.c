#include <stdlib.h>
#include <tommath.h>

typedef struct {
  mp_int num;
  mp_int den;
} sb_frac;

mp_err sb_frac_init(sb_frac *q) {
  return mp_init_multi(&q->num, &q->den, NULL);
}

void sb_frac_clear(sb_frac *q) { mp_clear_multi(&q->den, &q->num, NULL); }

// calcula a [mediante] de duas frações. é perfeitamente legal termos
// a == b ou b == c ou a == c
// [mediante](https://en.wikipedia.org/wiki/Mediant_(mathematics))
mp_err sb_mediant(sb_frac *a, sb_frac *b, sb_frac *c) {
  mp_err ret;
  ret = mp_add(&a->num, &b->num, &c->num);
  if (ret != MP_OKAY)
    return ret;
  ret = mp_add(&a->den, &b->den, &c->den);
  return ret;
}

// dá print na fração `a` em base `radix` em `stream`
mp_err sb_fwrite_frac(sb_frac *a, int radix, FILE *stream) {
  mp_err ret = mp_fwrite(&a->num, radix, stream);
  if (ret != MP_OKAY)
    return ret;
  fprintf(stream, "/");
  ret = mp_fwrite(&a->den, radix, stream);
  return ret;
}

struct sb_node {
  sb_frac frac;
  struct sb_node *l;
  struct sb_node *r;
};

typedef struct sb_node sb_node;

// #TODO fazer uma implementação não-recursiva disto tenho medo de
// stack-overflows, apesar de que acho muito difícil termos uma árvore tão
// profunda que vá dar stack overflow
void sb_tree_free(sb_node *node) {
  if (node->l != NULL) {
    sb_tree_free(node->l);
  }
  if (node->r != NULL) {
    sb_tree_free(node->r);
  }
  sb_frac_clear(&node->frac);
  free(node);
}
void sb_tree_clear(sb_node *node) {
  if (node->l != NULL) {
    sb_tree_free(node->l);
  }
  if (node->r != NULL) {
    sb_tree_free(node->r);
  }
  sb_frac_clear(&node->frac);
}

int main() {
  sb_frac p;
  sb_frac q;
  // #TODO verificar erros de inicialiação
  sb_frac_init(&p);
  sb_frac_init(&q);

  const int32_t one = 1;
  const int32_t zero = 0;

  mp_set_i32(&p.num, zero);
  mp_set_i32(&p.den, one);

  mp_set_i32(&q.num, one);
  mp_set_i32(&q.den, zero);

  sb_fwrite_frac(&p, 10, stdout);
  printf("\n");
  sb_fwrite_frac(&q, 10, stdout);
  printf("\n");

  sb_mediant(&p, &q, &p);
  sb_fwrite_frac(&p, 10, stdout);
  printf("\n");

  sb_frac_clear(&q);
}
