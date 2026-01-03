//
// Created by fnellen on 31.08.2025.
//

#ifndef SURGE_VV_UTILS_H
#define SURGE_VV_UTILS_H
#include "config.h"
#include "element.h"

/**
 * @brief Checks if the element at index `i` is hydrogen ("H").
 *
 * @param i Index into the global element array.
 * @return true if element[i].name is exactly "H", false otherwise.
 */
bool is_hydrogen(int i);

/**
 * @brief Write an unsigned long as decimal characters to a buffer.
 *
 * Appends the decimal representation of @p x at the current write position
 * and advances @p *p by the number of written characters.
 *
 * @param p  Pointer to a moving write cursor into a caller-owned buffer.
 * @param x  Unsigned long to print (base 10).
 */
void put_int(char **p, unsigned long x);

/**
 * @brief Write a single space character to a buffer.
 *
 * Appends ' ' at the current write position and advances @p *p by one.
 *
 * @param p  Pointer to a moving write cursor into a caller-owned buffer.
 */
void space(char **p);

/**
 * @brief Write a NUL-terminated C string to a buffer.
 *
 * Copies characters from @p s (excluding the terminating NUL) to the buffer
 * at the current write position and advances @p *p accordingly.
 *
 * @param p  Pointer to a moving write cursor into a caller-owned buffer.
 * @param s  NUL-terminated input string to append.
 */
void put_str(char **p, const char *s);

/**
 * @brief Write a bond marker character to a buffer.
 *
 * Encodes @p bnd as:
 *   - 0 -> '-' (single bond)
 *   - 1 -> '=' (double bond)
 *   - otherwise -> '#' (triple bond)
 * Appends the chosen character and advances @p *p by one.
 *
 * @param p   Pointer to a moving write cursor into a caller-owned buffer.
 * @param bnd Boundary code to encode.
 */
void put_bnd(char **p, int bnd);

int parse_int(const char *s, int *out);

int parse_long_pair_slash(const char *s, long *a, long *b);

int parse_long_pair_range(const char *s, long *a, long *b);

void set_badlist_from_csv(RingState *ring_state, const char *csv);

int nCk(int n, int k);

void vv_logf(SurgeCtx *ctx, const char *fmt, ...);

void surge_set_error_log_path(const char *path);

void surge_fatal(const char *msg);

void surge_dump_context(FILE *fp, const SurgeCtx *ctx);

void surge_print_error_log(void);

#endif //SURGE_VV_UTILS_H