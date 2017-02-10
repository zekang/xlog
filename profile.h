/*
+----------------------------------------------------------------------+
| PHP Version 5                                                        |
+----------------------------------------------------------------------+
| Copyright (c) 1997-2014 The PHP Group                                |
+----------------------------------------------------------------------+
| This source file is subject to version 3.01 of the PHP license,      |
| that is bundled with this package in the file LICENSE, and is        |
| available through the world-wide-web at the following url:           |
| http://www.php.net/license/3_01.txt                                  |
| If you did not receive a copy of the PHP license and are unable to   |
| obtain it through the world-wide-web, please send a note to          |
| license@php.net so we can mail you a copy immediately.               |
+----------------------------------------------------------------------+
| Author:  wanghouqian <whq654321@126.com>                             |
+----------------------------------------------------------------------+
*/
#ifndef PROFILE_H
#define PROFILE_H
typedef unsigned long long uint64;
typedef unsigned char uint8;
/**
* *****************************
* GLOBAL DATATYPES AND TYPEDEFS
* *****************************
*/

/* XHProf maintains a stack of entries being profiled. The memory for the entry
* is passed by the layer that invokes BEGIN_PROFILING(), e.g. the hp_execute()
* function. Often, this is just C-stack memory.
*
* This structure is a convenient place to track start time of a particular
* profile operation, recursion depth, and the name of the function being
* profiled. */
typedef struct hp_entry_t {
	char                   *name_hprof;                       /* function name */
	int                     rlvl_hprof;        /* recursion level for function */
	double                  tsc_start;         /* start micro_time  */
	long int                mu_start_hprof;                    /* memory usage */
	long int                pmu_start_hprof;              /* peak memory usage */
	struct hp_entry_t      *prev_hprof;    /* ptr to prev entry being profiled */
	uint8                   hash_code;     /* hash_code for the function name  */
} hp_entry_t;

/* Various types for XHPROF callbacks       */
typedef void(*hp_init_cb)           (TSRMLS_D);
typedef void(*hp_exit_cb)           (TSRMLS_D);
typedef void(*hp_begin_function_cb) (hp_entry_t **entries,
	hp_entry_t *current   TSRMLS_DC);
typedef void(*hp_end_function_cb)   (hp_entry_t **entries  TSRMLS_DC);

/* Struct to hold the various callbacks for a single xhprof mode */
typedef struct hp_mode_cb {
	hp_init_cb             init_cb;
	hp_exit_cb             exit_cb;
	hp_begin_function_cb   begin_fn_cb;
	hp_end_function_cb     end_fn_cb;
} hp_mode_cb;

/* Xhprof's global state.
*
* This structure is instantiated once.  Initialize defaults for attributes in
* hp_init_profiler_state() Cleanup/free attributes in
* hp_clean_profiler_state() */
typedef struct hp_global_t {

	/*       ----------   Global attributes:  -----------       */

	/* Indicates if xhprof is currently enabled */
	int              enabled;

	/* Indicates if xhprof was ever enabled during this request */
	int              ever_enabled;

	/* Holds all the xhprof statistics */
	zval            *stats_count;
	/* freelist of hp_entry_t chunks for reuse... */
	hp_entry_t      *entry_free_list;
	/* Top of the profile stack */
	hp_entry_t      *entries;

	/* Callbacks for various xhprof modes */
	hp_mode_cb       mode_cb;

	/* counter table indexed by hash value of function names. */
	uint8  func_hash_counters[256];

} hp_global_t;

/* XHProf global state */
hp_global_t       hp_globals;

void hp_begin(TSRMLS_D);
void hp_stop(TSRMLS_D);
void hp_end(TSRMLS_D);
uint8 hp_inline_hash(char *symbol);
hp_entry_t * hp_fast_alloc_hprof_entry();
void hp_fast_free_hprof_entry(hp_entry_t *p);
char * hp_get_function_name(zend_op_array *ops TSRMLS_DC);
void hp_mode_common_endfn(hp_entry_t **entries, hp_entry_t *current TSRMLS_DC);
void hp_mode_common_beginfn(hp_entry_t **entries,hp_entry_t  *current  TSRMLS_DC);
void hp_inc_count(zval *counts, char *name, long count TSRMLS_DC);
void hp_inc_count_wt(zval *counts, char *name, double count TSRMLS_DC);


double  cycle_timer();
zval * hp_hash_lookup(char *symbol  TSRMLS_DC);
void hp_free_the_free_list();
void profile_minit(TSRMLS_D);
void profile_mend(TSRMLS_D);
void profile_rend(TSRMLS_D);



#define MICRO_IN_SEC 1000000.00

#define ROOT_SYMBOL "main"
/* Size of a temp scratch buffer            */
#define SCRATCH_BUF_LEN            512


/*
* Start profiling - called just before calling the actual function
* NOTE:  PLEASE MAKE SURE TSRMLS_CC IS AVAILABLE IN THE CONTEXT
*        OF THE FUNCTION WHERE THIS MACRO IS CALLED.
*        TSRMLS_CC CAN BE MADE AVAILABLE VIA TSRMLS_DC IN THE
*        CALLING FUNCTION OR BY CALLING TSRMLS_FETCH()
*        TSRMLS_FETCH() IS RELATIVELY EXPENSIVE.
*/
#define BEGIN_PROFILING(entries, symbol, profile_curr)                  \
do { \
	/* Use a hash code to filter most of the string comparisons. */     \
	uint8 hash_code = hp_inline_hash(symbol);                          \
	profile_curr = 1;                 \
	if (profile_curr) { \
	hp_entry_t *cur_entry = hp_fast_alloc_hprof_entry();              \
	(cur_entry)->hash_code = hash_code;                               \
	(cur_entry)->name_hprof = symbol;                                 \
	(cur_entry)->prev_hprof = (*(entries));                           \
	/* Call the universal callback */                                 \
	hp_mode_common_beginfn((entries), (cur_entry)TSRMLS_CC);         \
	/* Call the mode's beginfn callback */                            \
	hp_globals.mode_cb.begin_fn_cb((entries), (cur_entry)TSRMLS_CC); \
	/* Update entries linked list */                                  \
	(*(entries)) = (cur_entry);                                       \
}                                                                   \
} while (0)

/*
* Stop profiling - called just after calling the actual function
* NOTE:  PLEASE MAKE SURE TSRMLS_CC IS AVAILABLE IN THE CONTEXT
*        OF THE FUNCTION WHERE THIS MACRO IS CALLED.
*        TSRMLS_CC CAN BE MADE AVAILABLE VIA TSRMLS_DC IN THE
*        CALLING FUNCTION OR BY CALLING TSRMLS_FETCH()
*        TSRMLS_FETCH() IS RELATIVELY EXPENSIVE.
*/
#define END_PROFILING(entries, profile_curr)                            \
do { \
	if (profile_curr) { \
		hp_entry_t *cur_entry;                                            \
		/* Call the mode's endfn callback. */                             \
		/* NOTE(cjiang): we want to call this 'end_fn_cb' before */       \
		/* 'hp_mode_common_endfn' to avoid including the time in */       \
		/* 'hp_mode_common_endfn' in the profiling results.      */       \
		hp_globals.mode_cb.end_fn_cb((entries)TSRMLS_CC);                \
		cur_entry = (*(entries));                                         \
		/* Call the universal callback */                                 \
		hp_mode_common_endfn((entries), (cur_entry)TSRMLS_CC);           \
		/* Free top entry and update entries linked list */               \
		(*(entries)) = (*(entries))->prev_hprof;                          \
		hp_fast_free_hprof_entry(cur_entry);                              \
	}                                                                   \
} while (0)



#endif
