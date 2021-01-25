#include <assert.h>
#include <stdlib.h>

#include <tommath.h>

// uma fração é um par de inteiros
typedef struct {
  mp_int num;
  mp_int den;
} sb_frac;

// um nó de árvore binária de stern-brocot
struct sb_node {
  sb_frac frac;
  struct sb_node *l;
  struct sb_node *r;
};
typedef struct sb_node sb_node;

// um intervalo limitado por frações diádicas com denominadores iguais a
// 2^den_exp. acho melhor deixar o intervalo inteiro na mesma struct porque
// assim não tenho que me preocupar em ficar sincronizando os denominadores
typedef struct {
  mp_int lower; // numerador do limite inferior
  mp_int upper; // numerador do limite superior

  mp_int comparer; // número que vamos reaproveitar para armazenar o resultado
                   // das multiplicações quando formos verificar se uma fração
                   // está dentro do intervalo ou não

  int32_t den_exp; // expoente do denominador, talvez eu mude para uint32_t se
                   // não precisar fazer muita aritmética com ele
} sb_dlimits;

mp_err sb_frac_init(sb_frac *q) {
  return mp_init_multi(&q->num, &q->den, NULL);
}

void sb_frac_clear(sb_frac *q) { mp_clear_multi(&q->den, &q->num, NULL); }

// calcula a [mediante] de duas frações. é perfeitamente legal termos
// a == b ou b == c ou a == c
// [mediante]: https://en.wikipedia.org/wiki/Mediant_(mathematics)
mp_err sb_mediant(sb_frac *a, sb_frac *b, sb_frac *c) {
  mp_err ret;
  ret = mp_add(&a->num, &b->num, &c->num);
  if (ret != MP_OKAY)
    return ret;
  ret = mp_add(&a->den, &b->den, &c->den);
  return ret;
}

// dá print na fração `a` em base `radix` em `stream`
mp_err sb_fwrite_frac(sb_frac *a, int32_t radix, FILE *stream) {
  mp_err ret = mp_fwrite(&a->num, radix, stream);
  if (ret != MP_OKAY)
    return ret;
  fprintf(stream, "/");
  ret = mp_fwrite(&a->den, radix, stream);
  return ret;
}

// #TODO fazer uma implementação não-recursiva disto. tenho medo de
// stack-overflows, apesar de que acho muito difícil termos uma árvore tão
// profunda que vá dar stack overflow
void sb_tree_free(sb_node *node) {
  if (node->l != NULL) {
    sb_tree_free(node->l);
    node->l = NULL;
  }
  if (node->r != NULL) {
    sb_tree_free(node->r);
    node->r = NULL;
  }
  sb_frac_clear(&node->frac);
  free(node);
  node = NULL;
}

void sb_tree_clear(sb_node *node) {
  if (node->l != NULL) {
    sb_tree_free(node->l);
    node->l = NULL;
  }
  if (node->r != NULL) {
    sb_tree_free(node->r);
    node->r = NULL;
  }
  sb_frac_clear(&node->frac);
}

// IMPORTANTE: isso pode vazar memória se você chamar numa árvore já existente
mp_err sb_tree_populate_with_neighbors(sb_node *node, int32_t depth,
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

// compara a fração frac com o limite inferior do intervalo `interval`
// retorna:
// * MP_GT se frac >  interval->lower
// * MP_EQ se frac == interval->lower
// * MP_LT se frac <  interval->lower
int sb_lcompare(sb_frac *frac, sb_dlimits *interval) {
  // aqui a vantagem de usar frações diádicas fica aparente, sempre temos
  // \frac{a}{b} < \frac{c}{d} \Leftrightarrow \frac{a \cdot d}{b} < c
  // é o único passo até agora que exige multiplicação, infelizmente, mas pelo
  // menos se temos b = 2^den_exp podemos substituir a divisão por um shift
  // pra direita, o que é muuuuito mais rápido do que dividir.

  // #TODO casos de erro you know the drill
  mp_mul(&interval->lower, &frac->den, &interval->comparer);
  mp_rshd(&interval->comparer, interval->den_exp);
  return mp_cmp(&frac->num, &interval->comparer);
}
// completamente análoga à função anterior, mas com interval->upper em vez de
// lower
int sb_ucompare(sb_frac *frac, sb_dlimits *interval) {
  // #TODO casos de erro you know the drill
  mp_mul(&interval->upper, &frac->den, &interval->comparer);
  mp_rshd(&interval->comparer, interval->den_exp);
  return mp_cmp(&frac->num, &interval->comparer);
}

sb_node *new_node(sb_frac *left_neighbor, sb_frac *right_neighbor) {
  sb_node *ret = malloc(sizeof(*ret));
  sb_frac_init(&ret->frac);
  sb_mediant(left_neighbor, right_neighbor, &ret->frac);
  // #TODO será que não existe uma forma de evitar ficar escrevendo esse NULL
  // quando ele vai ser substituido logo depois? provavelmente não faz diferença
  // pra performance, mas faz diferença pra mim
  ret->l = NULL;
  ret->r = NULL;
  return ret;
}

// gera a menor árvore que tem profundidade `depth` dentro de `limits`
mp_err sb_tree_populate_inside_limits(sb_node *node, int32_t depth,
                                      sb_frac *left_neighbor,
                                      sb_frac *right_neighbor,
                                      sb_dlimits *limits) {
  assert(mp_cmp(&limits->lower, &limits->upper) == MP_GT);

  if (depth > 0) {
    const int remaining_depth = depth - 1;

    // se estamos a esquerda do limite inferior, não temos motivo para ir mais a
    // esquerda
    const int lcmp_res = sb_lcompare(&node->frac, limits);
    if (lcmp_res != MP_LT) {
      if (node->l == NULL) {
        node->l = new_node(left_neighbor, &node->frac);
      }
      sb_tree_populate_inside_limits(node->l, remaining_depth, left_neighbor,
                                     &node->frac, limits);
    } else if (node->l != NULL) {
      sb_tree_free(node->l);
    }
    // da mesma forma, se estamos a direita do limite superior, não temos motivo
    // para ir mais para a direita
    const int ucmp_res = sb_ucompare(&node->frac, limits);
    if (ucmp_res != MP_GT) {
      if (node->r == NULL) {
        node->r = new_node(&node->frac, right_neighbor);
      }
      sb_tree_populate_inside_limits(node->r, remaining_depth, &node->frac,
                                     right_neighbor, limits);
    } else if (node->r != NULL) {
      sb_tree_free(node->r);
    }
    // em todo intervalo válido pelo menos um dos `if`s acima é executado
  } else {
    if (node->l != NULL) {
      sb_tree_free(node->l);
    }
    if (node->r != NULL) {
      sb_tree_free(node->r);
    }
  }
  return MP_OKAY;
}

mp_err sb_fwrite_tree(sb_node *node, int32_t radix, FILE *stream) {
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

mp_err sb_tree_populate(sb_node *node, int32_t depth) {
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

  const mp_err ret = sb_tree_populate_with_neighbors(node, depth, &p, &q);

  sb_frac_clear(&q);
  sb_frac_clear(&p);
  return ret;
}

int main(int argc, char *argv[]) {
  if (argc != 2)
    return 0;
  const int32_t depth = atoi(argv[1]);

  sb_node root;
  sb_frac_init(&root.frac);
  sb_tree_populate(&root, depth);
  sb_fwrite_tree(&root, 10, stdout);

  sb_tree_clear(&root);
}
