#include <stdlib.h>
#include <tommath.h>

// uma fração é um par de inteiros
typedef struct {
  mp_int num;
  mp_int den;
} sb_frac;

// uma fração diádica tem denominador igual a 2^den_exp
typedef struct {
  mp_int num;
  int32_t den_exp;
} sb_dfrac;

// um nó de árvore binária de stern-brocot
struct sb_node {
  sb_frac frac;
  struct sb_node *l;
  struct sb_node *r;
};

typedef struct sb_node sb_node;

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

mp_err sb_tree_populate_with_neighbors(sb_node *node, int depth,
                                       sb_frac *left_neighbor,
                                       sb_frac *right_neighbor) {
  mp_err ret = sb_mediant(left_neighbor, right_neighbor, &node->frac);
  if (ret != MP_OKAY) {
    node->l = node->r = NULL;
    return ret;
  }

  if (depth > 0) {
    // #TODO lidar com casos de erro
    node->l = malloc(sizeof(*node->l));
    sb_frac_init(&node->l->frac);
    sb_tree_populate_with_neighbors(node->l, depth - 1, left_neighbor,
                                    &node->frac);

    node->r = malloc(sizeof(*node->r));
    sb_frac_init(&node->r->frac);
    sb_tree_populate_with_neighbors(node->r, depth - 1, &node->frac,
                                    right_neighbor);
  } else {
    node->l = node->r = NULL;
  }
  return MP_OKAY;
}

mp_err sb_fwrite_tree(sb_node *node, int radix, FILE *stream) {
  // #TODO lidar com casos de erro
  if (node->l != NULL) {
    sb_fwrite_tree(node->l, radix, stream);
  }
  sb_fwrite_frac(&node->frac, radix, stream);
  fprintf(stream, "\n");
  if (node->r != NULL) {
    sb_fwrite_tree(node->r, radix, stream);
  }
  return MP_OKAY;
}

int main(int argc, char *argv[]) {
  if (argc != 2)
    return 0;
  const int depth = atoi(argv[1]);

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

  sb_node root;
  sb_frac_init(&root.frac);
  sb_tree_populate_with_neighbors(&root, depth, &p, &q);
  sb_fwrite_tree(&root, 10, stdout);

  sb_tree_clear(&root);

  sb_frac_clear(&q);
  sb_frac_clear(&p);
}
