#include <stdlib.h>
#include <tommath.h>

typedef struct
{
	mp_int num;
	mp_int den;
} sb_frac;

mp_err sb_frac_init(sb_frac *q)
{
	return mp_init_multi(&q->num, &q->den, NULL);
}

void sb_frac_clear(sb_frac *q)
{
	mp_clear_multi(&q->den, &q->num, NULL);
}

// calcula a [mediante] de duas frações
// é perfeitamente legal termos
// a == b ou b == c ou a == c
// [mediante](https://en.wikipedia.org/wiki/Mediant_(mathematics))
mp_err sb_mediant(sb_frac *a, sb_frac *b, sb_frac *c)
{
	mp_err ret;
	ret = mp_add(&a->num, &b->num, &c->num);
	if (ret != MP_OKAY) return ret;
	ret = mp_add(&a->den, &b->den, &c->den);
	return ret;
}

int main()
{
	sb_frac q;
	sb_frac_init(&q);
	sb_frac_clear(&q);
	/*
	mp_int *a = malloc(sizeof(*a));
	mp_int *b = malloc(sizeof(*b));

	mp_init(a);
	mp_init(b);

	int32_t one = 1;
	mp_set(a, one);
	mp_set(b, one);

	for (int i = 0; i < 1000000; i++)
	{
		// uma invariante do loop: a <= b /\ a > 0 /\ b > 0
		mp_add(a, b, a);
		mp_exch(a, b);

	}
	mp_fwrite(b, 10, stdout);
	printf("\n\n");

	mp_clear(b);
	mp_clear(a);

	free(b);
	free(a);
	*/
}
