#ifndef PKG_JSON_H
#define PKG_JSON_H
#include <stddef.h> /* size_t. */

typedef enum {
    PJ_NULL,
    PJ_BOOL,
    PJ_NUMBER,
    PJ_STRING,
    PJ_ARRAY,
    PJ_OBJECT
} pj_type;
typedef struct pj_value {
    pj_type type;
    char *text;
    char *key;
    struct pj_value *child;
    struct pj_value *next;
} pj_value;
/* Tree ownership transfers to the caller. Failure returns NULL. */
pj_value *pj_parse(const char *data, size_t length);
void pj_free(pj_value *value);
const pj_value *pj_get(const pj_value *object, const char *key);
size_t pj_count(const pj_value *value);
int pj_fields(const pj_value *object, const char *const *keys, size_t count);
#endif
