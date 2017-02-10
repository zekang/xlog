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
#define _CRT_SECURE_NO_WARNINGS
#include "php.h"
#include "zend_extensions.h"
#include "php_xlog.h"
#include "profile.h"


#ifdef PHP_WIN32
#include "win32/time.h"
#elif defined(NETWARE)
#include <sys/timeval.h>
#include <sys/time.h>
#else
#include <sys/time.h>
#endif




#if PHP_VERSION_ID < 50500
/* Pointer to the original execute function */
static ZEND_DLEXPORT void(*_zend_execute) (zend_op_array *ops TSRMLS_DC);
/* Pointer to the origianl execute_internal function */
static ZEND_DLEXPORT void(*_zend_execute_internal) (zend_execute_data *data,int ret TSRMLS_DC);
#else
/* Pointer to the original execute function */
static void(*_zend_execute_ex) (zend_execute_data *execute_data TSRMLS_DC);
/* Pointer to the origianl execute_internal function */
static void(*_zend_execute_internal) (zend_execute_data *data,struct _zend_fcall_info *fci, int ret TSRMLS_DC);
#endif

/* Pointer to the original compile function */
static zend_op_array * (*_zend_compile_file) (zend_file_handle *file_handle,int type TSRMLS_DC);
/* Pointer to the original compile string function (used by eval) */
static zend_op_array * (*_zend_compile_string) (zval *source_string, char *filename TSRMLS_DC);


/**
* A hash function to calculate a 8-bit hash code for a function name.
* This is based on a small modification to 'zend_inline_hash_func' by summing
* up all bytes of the ulong returned by 'zend_inline_hash_func'.
*
* @param str, char *, string to be calculated hash code for.
*
* @author cjiang
*/
inline uint8 hp_inline_hash(char * str) {
	ulong h = 5381;
	uint i = 0;
	uint8 res = 0;

	while (*str) {
		h += (h << 5);
		h ^= (ulong)*str++;
	}

	for (i = 0; i < sizeof(ulong); i++) {
		res += ((uint8 *)&h)[i];
	}
	return res;
}



/**
* ****************************
* XHPROF COMMON CALLBACKS
* ****************************
*/
/**
* XHPROF universal begin function.
* This function is called for all modes before the
* mode's specific begin_function callback is called.
*
* @param  hp_entry_t **entries  linked list (stack)
*                                  of hprof entries
* @param  hp_entry_t  *current  hprof entry for the current fn
* @return void
* @author kannan, veeve
*/
void hp_mode_common_beginfn(hp_entry_t **entries,
	hp_entry_t  *current  TSRMLS_DC) {
	hp_entry_t   *p;

	/* This symbol's recursive level */
	int    recurse_level = 0;

	if (hp_globals.func_hash_counters[current->hash_code] > 0) {
		/* Find this symbols recurse level */
		for (p = (*entries); p; p = p->prev_hprof) {
			if (!strcmp(current->name_hprof, p->name_hprof)) {
				recurse_level = (p->rlvl_hprof) + 1;
				break;
			}
		}
	}
	hp_globals.func_hash_counters[current->hash_code]++;

	/* Init current function's recurse level */
	current->rlvl_hprof = recurse_level;
}

/**
* XHPROF universal end function.  This function is called for all modes after
* the mode's specific end_function callback is called.
*
* @param  hp_entry_t **entries  linked list (stack) of hprof entries
* @return void
* @author kannan, veeve
*/
void hp_mode_common_endfn(hp_entry_t **entries, hp_entry_t *current TSRMLS_DC) {
	hp_globals.func_hash_counters[current->hash_code]--;
}

/**
* ***************************
* XHPROF DUMMY CALLBACKS
* ***************************
*/
void hp_mode_dummy_init_cb(TSRMLS_D) 
{
}


void hp_mode_dummy_exit_cb(TSRMLS_D) 
{ 
}


void hp_mode_dummy_beginfn_cb(hp_entry_t **entries,hp_entry_t *current  TSRMLS_DC) 
{ 
}

void hp_mode_dummy_endfn_cb(hp_entry_t **entries   TSRMLS_DC) 
{
}


/**
* Returns formatted function name
*
* @param  entry        hp_entry
* @param  result_buf   ptr to result buf
* @param  result_len   max size of result buf
* @return total size of the function name returned in result_buf
* @author veeve
*/
size_t hp_get_entry_name(hp_entry_t  *entry,
	char           *result_buf,
	size_t          result_len) {

	/* Validate result_len */
	if (result_len <= 1) {
		/* Insufficient result_bug. Bail! */
		return 0;
	}

	/* Add '@recurse_level' if required */
	/* NOTE:  Dont use snprintf's return val as it is compiler dependent */
	if (entry->rlvl_hprof) {
		snprintf(result_buf, result_len,
			"%s@%d",
			entry->name_hprof, entry->rlvl_hprof);
	}
	else {
		snprintf(result_buf, result_len,
			"%s",
			entry->name_hprof);
	}

	/* Force null-termination at MAX */
	result_buf[result_len - 1] = 0;

	return strlen(result_buf);
}


/**
* Build a caller qualified name for a callee.
*
* For example, if A() is caller for B(), then it returns "A==>B".
* Recursive invokations are denoted with @<n> where n is the recursion
* depth.
*
* For example, "foo==>foo@1", and "foo@2==>foo@3" are examples of direct
* recursion. And  "bar==>foo@1" is an example of an indirect recursive
* call to foo (implying the foo() is on the call stack some levels
* above).
*
* @author kannan, veeve
*/
size_t hp_get_function_stack(hp_entry_t *entry,
	int            level,
	char          *result_buf,
	size_t         result_len) {
	size_t         len = 0;

	/* End recursion if we dont need deeper levels or we dont have any deeper
	* levels */
	if (!entry->prev_hprof || (level <= 1)) {
		return hp_get_entry_name(entry, result_buf, result_len);
	}

	/* Take care of all ancestors first */
	len = hp_get_function_stack(entry->prev_hprof,
		level - 1,
		result_buf,
		result_len);

	/* Append the delimiter */
# define    HP_STACK_DELIM        "==>"
# define    HP_STACK_DELIM_LEN    (sizeof(HP_STACK_DELIM) - 1)

	if (result_len < (len + HP_STACK_DELIM_LEN)) {
		/* Insufficient result_buf. Bail out! */
		return len;
	}

	/* Add delimiter only if entry had ancestors */
	if (len) {
		strncat(result_buf + len,
			HP_STACK_DELIM,
			result_len - len);
		len += HP_STACK_DELIM_LEN;
	}

# undef     HP_STACK_DELIM_LEN
# undef     HP_STACK_DELIM

	/* Append the current function name */
	return len + hp_get_entry_name(entry,
		result_buf + len,
		result_len - len);
}


/**
* Takes an input of the form /a/b/c/d/foo.php and returns
* a pointer to one-level directory and basefile name
* (d/foo.php) in the same string.
*/
const char *hp_get_base_filename(const char *filename) {
	const char *ptr;
	int   found = 0;

	if (!filename)
		return "";

	/* reverse search for "/" and return a ptr to the next char */
	for (ptr = filename + strlen(filename) - 1; ptr >= filename; ptr--) {
		if (*ptr == '/') {
			found++;
		}
		if (found == 2) {
			return ptr + 1;
		}
	}

	/* no "/" char found, so return the whole string */
	return filename;
}


/**
* Get the name of the current function. The name is qualified with
* the class name if the function is in a class.
*
* @author kannan, hzhao
*/
char *hp_get_function_name(zend_op_array *ops TSRMLS_DC) {
	zend_execute_data *data;
	const char        *func = NULL;
	const char        *cls = NULL;
	char              *ret = NULL;
	int                len;
	zend_function      *curr_func;

	data = EG(current_execute_data);

	if (data) {
		/* shared meta data for function on the call stack */
		curr_func = data->function_state.function;

		/* extract function name from the meta info */
		func = curr_func->common.function_name;

		if (func) {
			/* previously, the order of the tests in the "if" below was
			* flipped, leading to incorrect function names in profiler
			* reports. When a method in a super-type is invoked the
			* profiler should qualify the function name with the super-type
			* class name (not the class name based on the run-time type
			* of the object.
			*/
			if (curr_func->common.scope) {
				cls = curr_func->common.scope->name;
			}
			else if (data->object) {
				cls = Z_OBJCE(*data->object)->name;
			}

			if (cls) {
				len = strlen(cls) + strlen(func) + 10;
				ret = (char*)emalloc(len);
				snprintf(ret, len, "%s::%s", cls, func);
			}
			else {
				ret = estrdup(func);
			}
		}
		else {
			long     curr_op;
			int      add_filename = 0;
			struct _zend_op *opline = data->opline;
			int i = 1000;
			while (opline->opcode != ZEND_INCLUDE_OR_EVAL && i > 0 ){
				i--;
				if (opline->opcode == ZEND_NOP){
					return NULL;
				}
				opline++;
			}
			if(opline->opcode != ZEND_INCLUDE_OR_EVAL){
				return NULL;
			}
			/* we are dealing with a special directive/function like
			* include, eval, etc.
			*/
#if ZEND_EXTENSION_API_NO >= 220100525
			curr_op = opline->extended_value;
#else
			curr_op = opline->op2.u.constant.value.lval;
#endif

			switch (curr_op) {
			case ZEND_EVAL:
				func = "eval";
				break;
			case ZEND_INCLUDE:
				func = "include";
				add_filename = 1;
				break;
			case ZEND_REQUIRE:
				func = "require";
				add_filename = 1;
				break;
			case ZEND_INCLUDE_ONCE:
				func = "include_once";
				add_filename = 1;
				break;
			case ZEND_REQUIRE_ONCE:
				func = "require_once";
				add_filename = 1;
				break;
			default:
				func = "unknown_op";
				break;
			}

			/* For some operations, we'll add the filename as part of the function
			* name to make the reports more useful. So rather than just "include"
			* you'll see something like "run_init::foo.php" in your reports.
			*/
			if (add_filename){
				const char *filename;
				int   len;
				filename = hp_get_base_filename((curr_func->op_array).filename);
				len = strlen(func) + strlen(filename) + 3;
				ret = (char *)emalloc(len);
				snprintf(ret, len, "%s::%s", func,filename);
			}
			else {
				ret = estrdup(func);
			}
		}
	}
	return ret;
}



/**
* Fast allocate a hp_entry_t structure. Picks one from the
* free list if available, else does an actual allocate.
*
* Doesn't bother initializing allocated memory.
*
* @author kannan
*/
hp_entry_t *hp_fast_alloc_hprof_entry() {
	hp_entry_t *p;

	p = hp_globals.entry_free_list;

	if (p) {
		hp_globals.entry_free_list = p->prev_hprof;
		return p;
	}
	else {
		return (hp_entry_t *)malloc(sizeof(hp_entry_t));
	}
}
/**
* Fast free a hp_entry_t structure. Simply returns back
* the hp_entry_t to a free list and doesn't actually
* perform the free.
*
* @author kannan
*/
void hp_fast_free_hprof_entry(hp_entry_t *p) {
	/* we use/overload the prev_hprof field in the structure to link entries in
	* the free list. */
	p->prev_hprof = hp_globals.entry_free_list;
	hp_globals.entry_free_list = p;
}

/**
* Free any items in the free list.
*/
void hp_free_the_free_list() {
	hp_entry_t *p = hp_globals.entry_free_list;
	hp_entry_t *cur;

	while (p) {
		cur = p;
		p = p->prev_hprof;
		free(cur);
		cur = NULL;
	}
}
/**
* Cleanup profiler state
*
* @author kannan, veeve
*/
void hp_clean_profiler_state(TSRMLS_D) {
	/* Call current mode's exit cb */
	hp_globals.mode_cb.exit_cb(TSRMLS_C);

	/* Clear globals */
	if (hp_globals.stats_count) {
		zval_dtor(hp_globals.stats_count);
		FREE_ZVAL(hp_globals.stats_count);
		hp_globals.stats_count = NULL;
	}
	hp_globals.entries = NULL;
	hp_globals.ever_enabled = 0;
}
/**
* ***************************
* PHP EXECUTE/COMPILE PROXIES
* ***************************
*/

/**
* XHProf enable replaced the zend_execute function with this
* new execute function. We can do whatever profiling we need to
* before and after calling the actual zend_execute().
*
* @author hzhao, kannan
*/
#if PHP_VERSION_ID < 50500
ZEND_DLEXPORT void hp_execute(zend_op_array *ops TSRMLS_DC) {
#else
ZEND_DLEXPORT void hp_execute_ex(zend_execute_data *execute_data TSRMLS_DC) {
	zend_op_array *ops = execute_data->op_array;
#endif
	char          *func = NULL;
	int hp_profile_flag = 1;

	func = hp_get_function_name(ops TSRMLS_CC);
	if (!func) {
#if PHP_VERSION_ID < 50500
		_zend_execute(ops TSRMLS_CC);
#else
		_zend_execute_ex(execute_data TSRMLS_CC);
#endif
		return;
	}

	BEGIN_PROFILING(&hp_globals.entries, func, hp_profile_flag);
#if PHP_VERSION_ID < 50500
	_zend_execute(ops TSRMLS_CC);
#else
	_zend_execute_ex(execute_data TSRMLS_CC);
#endif
	if (hp_globals.entries) {
		END_PROFILING(&hp_globals.entries, hp_profile_flag);
	}
	efree(func);
}

#undef EX
#define EX(element) ((execute_data)->element)

/**
* Very similar to hp_execute. Proxy for zend_execute_internal().
* Applies to zend builtin functions.
*
* @author hzhao, kannan
*/

#if PHP_VERSION_ID < 50500
#define EX_T(offset) (*(temp_variable *)((char *) EX(Ts) + offset))

ZEND_DLEXPORT void hp_execute_internal(zend_execute_data *execute_data,
	int ret TSRMLS_DC) {
#else
#define EX_T(offset) (*EX_TMP_VAR(execute_data, offset))

ZEND_DLEXPORT void hp_execute_internal(zend_execute_data *execute_data,
struct _zend_fcall_info *fci, int ret TSRMLS_DC) {
#endif
	zend_execute_data *current_data;
	char             *func = NULL;
	int    hp_profile_flag = 1;

	current_data = EG(current_execute_data);
	func = hp_get_function_name(current_data->op_array TSRMLS_CC);

	if (func) {
		BEGIN_PROFILING(&hp_globals.entries, func, hp_profile_flag);
	}

	if (!_zend_execute_internal) {
		/* no old override to begin with. so invoke the builtin's implementation  */
		zend_op *opline = EX(opline);
#if ZEND_EXTENSION_API_NO >= 220100525
		if (opline){
			temp_variable *retvar = &EX_T(opline->result.var);
			((zend_internal_function *)EX(function_state).function)->handler(
				opline->extended_value,
				retvar->var.ptr,
				(EX(function_state).function->common.fn_flags & ZEND_ACC_RETURN_REFERENCE) ?
				&retvar->var.ptr : NULL,
				EX(object), ret TSRMLS_CC);
		}else{
			((zend_internal_function *) EX(function_state).function)->handler(fci->param_count, *fci->retval_ptr_ptr, fci->retval_ptr_ptr, fci->object_ptr, 1 TSRMLS_CC);

		}
		
#else
		((zend_internal_function *)EX(function_state).function)->handler(
			opline->extended_value,
			EX_T(opline->result.u.var).var.ptr,
			EX(function_state).function->common.return_reference ?
			&EX_T(opline->result.u.var).var.ptr : NULL,
			EX(object), ret TSRMLS_CC);
#endif
	}
	else {
		/* call the old override */
#if PHP_VERSION_ID < 50500
		_zend_execute_internal(execute_data, ret TSRMLS_CC);
#else
		_zend_execute_internal(execute_data, fci, ret TSRMLS_CC);
#endif
	}

	if (func) {
		if (hp_globals.entries) {
			END_PROFILING(&hp_globals.entries, hp_profile_flag);
		}
		efree(func);
	}

}

/**
* Proxy for zend_compile_file(). Used to profile PHP compilation time.
*
* @author kannan, hzhao
*/
ZEND_DLEXPORT zend_op_array* hp_compile_file(zend_file_handle *file_handle,
	int type TSRMLS_DC) {

	const char     *filename;
	char           *func;
	int             len;
	zend_op_array  *ret;
	int             hp_profile_flag = 1;

	filename = hp_get_base_filename(file_handle->filename);
	len = strlen("load") + strlen(filename) + 3;
	func = (char *)emalloc(len);
	snprintf(func, len, "load::%s", filename);

	BEGIN_PROFILING(&hp_globals.entries, func, hp_profile_flag);
	ret = _zend_compile_file(file_handle, type TSRMLS_CC);
	if (hp_globals.entries) {
		END_PROFILING(&hp_globals.entries, hp_profile_flag);
	}

	efree(func);
	return ret;
}

/**
* Proxy for zend_compile_string(). Used to profile PHP eval compilation time.
*/
ZEND_DLEXPORT zend_op_array* hp_compile_string(zval *source_string, char *filename TSRMLS_DC) {

	char          *func;
	int            len;
	zend_op_array *ret;
	int            hp_profile_flag = 1;

	len = strlen("eval") + strlen(filename) + 3;
	func = (char *)emalloc(len);
	snprintf(func, len, "eval::%s", filename);

	BEGIN_PROFILING(&hp_globals.entries, func, hp_profile_flag);
	ret = _zend_compile_string(source_string, filename TSRMLS_CC);
	if (hp_globals.entries) {
		END_PROFILING(&hp_globals.entries, hp_profile_flag);
	}

	efree(func);
	return ret;
}


double  cycle_timer()
{
	struct timeval tp = { 0 };
	if (gettimeofday(&tp, NULL)){
		return 0;
	}
	return (double)tp.tv_sec + tp.tv_usec / MICRO_IN_SEC;
}

/**
* XHPROF_MODE_HIERARCHICAL's begin function callback
*
* @author kannan
*/
void hp_mode_hier_beginfn_cb(hp_entry_t **entries,
	hp_entry_t  *current  TSRMLS_DC) {
	/* Get start tsc counter */
	current->tsc_start = cycle_timer();
	/* Get memory usage */
	current->mu_start_hprof = zend_memory_usage(0 TSRMLS_CC);
	current->pmu_start_hprof = zend_memory_peak_usage(0 TSRMLS_CC);
}


/**
* Looksup the hash table for the given symbol
* Initializes a new array() if symbol is not present
*
* @author kannan, veeve
*/
zval * hp_hash_lookup(char *symbol  TSRMLS_DC) {
	HashTable   *ht;
	void        *data;
	zval        *counts = (zval *)0;

	/* Bail if something is goofy */
	if (!hp_globals.stats_count || !(ht = HASH_OF(hp_globals.stats_count))) {
		return (zval *)0;
	}

	/* Lookup our hash table */
	if (zend_hash_find(ht, symbol, strlen(symbol) + 1, &data) == SUCCESS) {
		/* Symbol already exists */
		counts = *(zval **)data;
	}
	else {
		/* Add symbol to hash table */
		MAKE_STD_ZVAL(counts);
		array_init(counts);
		add_assoc_zval(hp_globals.stats_count, symbol, counts);
	}

	return counts;
}

/**
* XHPROF shared end function callback
*
* @author kannan
*/
zval * hp_mode_shared_endfn_cb(hp_entry_t *top,
	char          *symbol  TSRMLS_DC) {
	zval    *counts;
	double   tsc_end;

	/* Get end tsc counter */
	tsc_end = cycle_timer();

	/* Get the stat array */
	if (!(counts = hp_hash_lookup(symbol TSRMLS_CC))) {
		return (zval *)0;
	}

	/* Bump stats in the counts hashtable */
	hp_inc_count(counts, "ct", 1  TSRMLS_CC);

	hp_inc_count_wt(counts, "wt", (tsc_end - top->tsc_start) TSRMLS_CC);

	return counts;
}


/**
* Increment the count of the given stat with the given count
* If the stat was not set before, inits the stat to the given count
*
* @param  zval *counts   Zend hash table pointer
* @param  char *name     Name of the stat
* @param  long  count    Value of the stat to incr by
* @return void
* @author kannan
*/
void hp_inc_count(zval *counts, char *name, long count TSRMLS_DC) {
	HashTable *ht;
	void *data;

	if (!counts) return;
	ht = HASH_OF(counts);
	if (!ht) return;

	if (zend_hash_find(ht, name, strlen(name) + 1, &data) == SUCCESS) {
		ZVAL_LONG(*(zval**)data, Z_LVAL_PP((zval**)data) + count);
	}
	else {
		add_assoc_long(counts, name, count);
	}
}

void hp_inc_count_wt(zval *counts, char *name, double count TSRMLS_DC) {
	HashTable *ht;
	void *data;

	if (!counts) return;
	ht = HASH_OF(counts);
	if (!ht) return;

	if (zend_hash_find(ht, name, strlen(name) + 1, &data) == SUCCESS) {
		ZVAL_DOUBLE(*(zval**)data, Z_DVAL_PP((zval**)data) + count);
	}
	else {
		add_assoc_double(counts, name, count);
	}
}

/**
* XHPROF_MODE_HIERARCHICAL's end function callback
*
* @author kannan
*/
void hp_mode_hier_endfn_cb(hp_entry_t **entries  TSRMLS_DC) {
	hp_entry_t   *top = (*entries);
	zval            *counts;
	char             symbol[SCRATCH_BUF_LEN];
	long int         mu_end;
	long int         pmu_end;

	/* Get the stat array */
	hp_get_function_stack(top, 2, symbol, sizeof(symbol));
	if (!(counts = hp_mode_shared_endfn_cb(top,
		symbol  TSRMLS_CC))) {
		return;
	}

	/* Get Memory usage */
	mu_end = zend_memory_usage(0 TSRMLS_CC);
	pmu_end = zend_memory_peak_usage(0 TSRMLS_CC);

	/* Bump Memory stats in the counts hashtable */
	hp_inc_count(counts, "mu", mu_end - top->mu_start_hprof    TSRMLS_CC);
	hp_inc_count(counts, "pmu", pmu_end - top->pmu_start_hprof  TSRMLS_CC);
}


/**
* Initialize profiler state
*
* @author kannan, veeve
*/
void hp_init_profiler_state(TSRMLS_D) {
	/* Setup globals */
	if (!hp_globals.ever_enabled) {
		hp_globals.ever_enabled = 1;
		hp_globals.entries = NULL;
	}


	/* Init stats_count */
	if (hp_globals.stats_count) {
		zval_dtor(hp_globals.stats_count);
		FREE_ZVAL(hp_globals.stats_count);
	}
	MAKE_STD_ZVAL(hp_globals.stats_count);
	array_init(hp_globals.stats_count);

	/* Call current mode's init cb */
	hp_globals.mode_cb.init_cb(TSRMLS_C);
}

/**
* **************************
* MAIN XHPROF CALLBACKS
* **************************
*/


/**
* This function gets called once when xhprof gets enabled.
* It replaces all the functions like zend_execute, zend_execute_internal,
* etc that needs to be instrumented with their corresponding proxies.
*/
 void hp_begin(TSRMLS_D) {
	if (!hp_globals.enabled) {
		int hp_profile_flag = 1;
		hp_globals.enabled = 1;
	

		/* Replace zend_compile with our proxy */
		_zend_compile_file = zend_compile_file;
		zend_compile_file = hp_compile_file;

		/* Replace zend_compile_string with our proxy */
		_zend_compile_string = zend_compile_string;
		zend_compile_string = hp_compile_string;

		/* Replace zend_execute with our proxy */
#if PHP_VERSION_ID < 50500
		_zend_execute = zend_execute;
		zend_execute = hp_execute;
#else
		_zend_execute_ex = zend_execute_ex;
		zend_execute_ex = hp_execute_ex;
#endif

		/* Replace zend_execute_internal with our proxy */
		_zend_execute_internal = zend_execute_internal;
		zend_execute_internal = hp_execute_internal;

		/* Initialize with the dummy mode first Having these dummy callbacks saves
		* us from checking if any of the callbacks are NULL everywhere. */
		hp_globals.mode_cb.init_cb = hp_mode_dummy_init_cb;
		hp_globals.mode_cb.exit_cb = hp_mode_dummy_exit_cb;
		hp_globals.mode_cb.begin_fn_cb = hp_mode_dummy_beginfn_cb;
		hp_globals.mode_cb.end_fn_cb = hp_mode_dummy_endfn_cb;

		hp_globals.mode_cb.begin_fn_cb = hp_mode_hier_beginfn_cb;
		hp_globals.mode_cb.end_fn_cb = hp_mode_hier_endfn_cb;

		/* one time initializations */
		hp_init_profiler_state(TSRMLS_C);

		/* start profiling from fictitious main() */
		BEGIN_PROFILING(&hp_globals.entries, ROOT_SYMBOL, hp_profile_flag);
	}
}

/**
* Called at request shutdown time. Cleans the profiler's global state.
*/
 void hp_end(TSRMLS_D) {
	/* Bail if not ever enabled */
	if (!hp_globals.ever_enabled) {
		return;
	}

	/* Stop profiler if enabled */
	if (hp_globals.enabled) {
		hp_stop(TSRMLS_C);
	}

	/* Clean up state */
	hp_clean_profiler_state(TSRMLS_C);
}

/**
* Called from xhprof_disable(). Removes all the proxies setup by
* hp_begin() and restores the original values.
*/
 void hp_stop(TSRMLS_D) {
	int   hp_profile_flag = 1;

	/* End any unfinished calls */
	while (hp_globals.entries) {
		END_PROFILING(&hp_globals.entries, hp_profile_flag);
	}

	/* Remove proxies, restore the originals */
#if PHP_VERSION_ID < 50500
	zend_execute = _zend_execute;
#else
	zend_execute_ex = _zend_execute_ex;
#endif
	zend_execute_internal = _zend_execute_internal;
	zend_compile_file = _zend_compile_file;
	zend_compile_string = _zend_compile_string;

	/* Stop profiling */
	hp_globals.enabled = 0;
}

void profile_minit(TSRMLS_D)
{
	int i;
	hp_globals.stats_count = NULL;

	/* no free hp_entry_t structures to start with */
	hp_globals.entry_free_list = NULL;

	for (i = 0; i < 256; i++) {
		hp_globals.func_hash_counters[i] = 0;
	}

}

void profile_mend(TSRMLS_D)
{
	hp_free_the_free_list();
}

void profile_rend(TSRMLS_D)
{
	hp_end(TSRMLS_C);
}
