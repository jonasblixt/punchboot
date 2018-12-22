#ifndef __GCOV_H__
#define __GCOV_H__

#include <stdint.h>

typedef uint64_t gcov_type;

#if (__GNUC__ >= 7)
#define GCOV_COUNTERS			9
#elif (__GNUC__ > 5) || (__GNUC__ == 5 && __GNUC_MINOR__ >= 1)
#define GCOV_COUNTERS			10
#elif __GNUC__ == 4 && __GNUC_MINOR__ >= 9
#define GCOV_COUNTERS			9
#else
#define GCOV_COUNTERS			8
#endif

#define GCOV_TAG_FUNCTION_LENGTH	3


struct gcov_ctr_info {
	unsigned int num;
	gcov_type *values;
};


struct gcov_fn_info {
	const struct gcov_info *key;
	unsigned int ident;
	unsigned int lineno_checksum;
	unsigned int cfg_checksum;
	struct gcov_ctr_info ctrs[1];
};


struct gcov_info {
	unsigned int version;
	struct gcov_info *next;
	unsigned int stamp;
	const char *filename;
	void (*merge[GCOV_COUNTERS])(gcov_type *, unsigned int);
	unsigned int n_functions;
	struct gcov_fn_info **functions;
};

void gcov_init(void);
uint32_t gcov_final(void);

#endif
