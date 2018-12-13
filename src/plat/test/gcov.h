#ifndef __GCOV_H__
#define __GCOV_H__

#include <stdint.h>

typedef long gcov_type;

struct gcov_fn_info {
	unsigned int ident;
	unsigned int checksum;
	unsigned int n_ctrs;
};

struct gcov_ctr_info {
	unsigned int	num;
	gcov_type	*values;
	void		(*merge)(gcov_type *, unsigned int);
};

struct gcov_info {
	unsigned int				version;
	struct gcov_info			*next;
	unsigned int				stamp;
	const char					*filename;
	unsigned int				n_functions;
	const struct gcov_fn_info	*functions;
	unsigned int				ctr_mask;
	struct gcov_ctr_info		counts;
};

void gcov_init(void);
uint32_t gcov_final(void);

#endif
