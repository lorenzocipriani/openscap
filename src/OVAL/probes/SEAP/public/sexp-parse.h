#pragma once
#ifndef SEXP_PARSE_H
#define SEXP_PARSE_H

#include <stddef.h>
#include <seap-debug.h>
#include <sexp-types.h>

typedef struct SEXP_psetup SEXP_psetup_t;

SEXP_psetup_t *SEXP_psetup_new  (void);
void           SEXP_psetup_free (SEXP_psetup_t *);

typedef struct SEXP_pstate SEXP_pstate_t;

SEXP_pstate_t *SEXP_pstate_new (void);
void           SEXP_pstate_free (SEXP_pstate_t *);

SEXP_t *SEXP_parse (const SEXP_psetup_t *setup, const char *buf, size_t buflen, SEXP_pstate_t **pstate);

#endif /* SEXP_PARSE_H */
