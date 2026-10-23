#include "pkg_json.h" /* pj_value and parser API. */
#include <stdint.h>   /* uint32_t. */
#include <stdlib.h>   /* calloc, malloc, free. */
#include <string.h>   /* memcpy, memcmp, memchr, strcmp, strchr, strlen. */

typedef struct {
    const unsigned char *p;
    const unsigned char *end;
} parser;

/* Purpose: Release a whole owned tree, including sibling nodes.
 * Args: value is an owned tree or NULL.
 * Rets: void.
 */
void pj_free(pj_value *value)
{
    while (value != NULL) {
        pj_value *next = value->next;
        pj_free(value->child);
        free(value->text);
        free(value->key);
        free(value);
        value = next;
    }
}

/* Purpose: Skip only legal JSON whitespace.
 * Args: s is the bounded parser.
 * Rets: void.
 */
static void space(parser *s)
{
    while (s->p < s->end && strchr(" \t\r\n", *s->p) != NULL)
        ++s->p;
}

/* Purpose: Decode one Unicode scalar from raw UTF-8.
 * Args: s is the parser; cp receives the scalar.
 * Rets: 0 success, -1 invalid or incomplete input.
 */
static int scalar(parser *s, uint32_t *cp)
{
    if (s->p == s->end)
        return -1;
    unsigned int first = *s->p++;
    unsigned int n;
    uint32_t minimum;
    if (first < 0x80) {
        *cp = first;
        return 0;
    }
    if (first >= 0xc2 && first <= 0xdf) {
        n = 1;
        minimum = 0x80;
        first &= 0x1f;
    } else if (first >= 0xe0 && first <= 0xef) {
        n = 2;
        minimum = 0x800;
        first &= 0x0f;
    } else if (first >= 0xf0 && first <= 0xf4) {
        n = 3;
        minimum = 0x10000;
        first &= 7;
    } else {
        return -1;
    }
    uint32_t code = first;
    for (unsigned int i = 0; i < n; ++i) {
        if (s->p == s->end || (*s->p & 0xc0) != 0x80)
            return -1;
        code = (code << 6) | (*s->p++ & 0x3f);
    }
    if (code < minimum || code > 0x10ffff || (code >= 0xd800 && code <= 0xdfff))
        return -1;
    *cp = code;
    return 0;
}

/* Purpose: Read exactly four hexadecimal digits.
 * Args: s is the parser; cp receives the code unit.
 * Rets: 0 success, -1 invalid input.
 */
static int hex4(parser *s, uint32_t *cp)
{
    uint32_t code = 0;
    for (int i = 0; i < 4; ++i) {
        if (s->p == s->end)
            return -1;
        unsigned int c = *s->p++;
        unsigned int v;
        if (c >= '0' && c <= '9')
            v = c - '0';
        else if (c >= 'a' && c <= 'f')
            v = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F')
            v = c - 'A' + 10;
        else
            return -1;
        code = (code << 4) | v;
    }
    *cp = code;
    return 0;
}

/* Purpose: Decode a JSON string to valid NUL-free UTF-8.
 * Args: s starts at a quote.
 * Rets: Owned decoded bytes, or NULL on failure.
 */
static char *string(parser *s)
{
    if (s->p == s->end || *s->p++ != '"')
        return NULL;
    const unsigned char *end = s->p;
    while (end < s->end && *end != '"') {
        if (*end++ == '\\' && end < s->end)
            ++end;
    }
    if (end == s->end)
        return NULL;
    /* UTF-8 output is never larger than this raw JSON string span. */
    char *out = malloc((size_t)(end - s->p) + 1);
    if (out == NULL)
        return NULL;
    size_t used = 0;
    while (s->p < s->end && *s->p != '"') {
        uint32_t cp;
        if (*s->p == '\\') {
            ++s->p;
            if (s->p == s->end)
                goto fail;
            unsigned char c = *s->p++;
            static const char escapes[] = "\"\\/bfnrt";
            static const char decoded[] = "\"\\/\b\f\n\r\t";
            const char *at = strchr(escapes, c);
            if (c == 'u') {
                if (hex4(s, &cp) != 0)
                    goto fail;
                if (cp >= 0xd800 && cp <= 0xdbff) {
                    uint32_t low;
                    if (s->end - s->p < 2 || s->p[0] != '\\' || s->p[1] != 'u')
                        goto fail;
                    s->p += 2;
                    if (hex4(s, &low) != 0 || low < 0xdc00 || low > 0xdfff)
                        goto fail;
                    cp = 0x10000 + ((cp - 0xd800) << 10) + low - 0xdc00;
                } else if (cp >= 0xdc00 && cp <= 0xdfff) {
                    goto fail;
                }
            } else if (at != NULL && c != 0) {
                cp = (unsigned char)decoded[at - escapes];
            } else {
                goto fail;
            }
        } else {
            if (*s->p < 0x20 || scalar(s, &cp) != 0)
                goto fail;
        }
        if (cp == 0)
            goto fail;
        if (cp < 0x80)
            out[used++] = (char)cp;
        else if (cp < 0x800) {
            out[used++] = (char)(0xc0 | (cp >> 6));
            out[used++] = (char)(0x80 | (cp & 0x3f));
        } else if (cp < 0x10000) {
            out[used++] = (char)(0xe0 | (cp >> 12));
            out[used++] = (char)(0x80 | ((cp >> 6) & 0x3f));
            out[used++] = (char)(0x80 | (cp & 0x3f));
        } else {
            out[used++] = (char)(0xf0 | (cp >> 18));
            out[used++] = (char)(0x80 | ((cp >> 12) & 0x3f));
            out[used++] = (char)(0x80 | ((cp >> 6) & 0x3f));
            out[used++] = (char)(0x80 | (cp & 0x3f));
        }
    }
    if (s->p == s->end)
        goto fail;
    ++s->p;
    out[used] = '\0';
    return out;
fail:
    free(out);
    return NULL;
}

/* Purpose: Test an ASCII decimal digit without locale state.
 * Args: c is a byte.
 * Rets: Nonzero for a decimal digit.
 */
static int digit(unsigned char c) { return c >= '0' && c <= '9'; }

/* Purpose: Parse one strict JSON value and its bounded descendants.
 * Args: s is the parser; depth counts open containers.
 * Rets: An owned node or NULL on failure.
 */
static pj_value *value(parser *s, unsigned int depth)
{
    space(s);
    if (s->p == s->end)
        return NULL;
    pj_value *v = calloc(1, sizeof(*v));
    if (v == NULL)
        return NULL;
    unsigned char first = *s->p;
    if (first == '{' || first == '[') {
        if (depth >= 16)
            goto fail;
        v->type = first == '{' ? PJ_OBJECT : PJ_ARRAY;
        unsigned char close = first == '{' ? '}' : ']';
        ++s->p;
        space(s);
        pj_value **tail = &v->child;
        if (s->p < s->end && *s->p == close) {
            ++s->p;
            return v;
        }
        for (;;) {
            char *key = NULL;
            if (first == '{') {
                key = string(s);
                if (key == NULL)
                    goto fail;
                for (pj_value *p = v->child; p != NULL; p = p->next) {
                    if (strcmp(p->key, key) == 0) {
                        free(key);
                        goto fail;
                    }
                }
                space(s);
                if (s->p == s->end || *s->p++ != ':') {
                    free(key);
                    goto fail;
                }
            }
            pj_value *item = value(s, depth + 1);
            if (item == NULL) {
                free(key);
                goto fail;
            }
            item->key = key;
            *tail = item;
            tail = &item->next;
            space(s);
            if (s->p == s->end)
                goto fail;
            if (*s->p == close) {
                ++s->p;
                return v;
            }
            if (*s->p++ != ',')
                goto fail;
            space(s);
        }
    }
    if (first == '"') {
        v->type = PJ_STRING;
        v->text = string(s);
        if (v->text == NULL)
            goto fail;
        return v;
    }
    const unsigned char *start = s->p;
    const char *word = NULL;
    if (first == 'n') {
        word = "null";
        v->type = PJ_NULL;
    }
    if (first == 't') {
        word = "true";
        v->type = PJ_BOOL;
    }
    if (first == 'f') {
        word = "false";
        v->type = PJ_BOOL;
    }
    if (word != NULL) {
        size_t n = strlen(word);
        if ((size_t)(s->end - s->p) < n || memcmp(s->p, word, n) != 0)
            goto fail;
        s->p += n;
    } else {
        v->type = PJ_NUMBER;
        if (*s->p == '-')
            ++s->p;
        if (s->p == s->end || !digit(*s->p))
            goto fail;
        if (*s->p == '0')
            ++s->p;
        else
            while (s->p < s->end && digit(*s->p))
                ++s->p;
        if (s->p < s->end && *s->p == '.') {
            ++s->p;
            if (s->p == s->end || !digit(*s->p))
                goto fail;
            while (s->p < s->end && digit(*s->p))
                ++s->p;
        }
        if (s->p < s->end && (*s->p == 'e' || *s->p == 'E')) {
            ++s->p;
            if (s->p < s->end && (*s->p == '+' || *s->p == '-'))
                ++s->p;
            if (s->p == s->end || !digit(*s->p))
                goto fail;
            while (s->p < s->end && digit(*s->p))
                ++s->p;
        }
    }
    size_t n = (size_t)(s->p - start);
    v->text = malloc(n + 1);
    if (v->text == NULL)
        goto fail;
    memcpy(v->text, start, n);
    v->text[n] = '\0';
    return v;
fail:
    pj_free(v);
    return NULL;
}

/* Purpose: Parse at most 256 KiB of strict JSON without trailing data.
 * Args: data is a borrowed byte span, length is its exact size.
 * Rets: Owned tree, or NULL for malformed input or allocation failure.
 */
pj_value *pj_parse(const char *data, size_t length)
{
    if (data == NULL || length == 0 || length > 262144 ||
        memchr(data, 0, length) != NULL)
        return NULL;
    parser s = {(const unsigned char *)data,
                (const unsigned char *)data + length};
    pj_value *v = value(&s, 0);
    space(&s);
    if (s.p != s.end) {
        pj_free(v);
        return NULL;
    }
    return v;
}

/* Purpose: Find one exact object key.
 * Args: object and key are borrowed.
 * Rets: Borrowed child or NULL.
 */
const pj_value *pj_get(const pj_value *object, const char *key)
{
    if (object == NULL || object->type != PJ_OBJECT || key == NULL)
        return NULL;
    for (const pj_value *p = object->child; p != NULL; p = p->next)
        if (strcmp(p->key, key) == 0)
            return p;
    return NULL;
}

/* Purpose: Count immediate child entries.
 * Args: value is borrowed or NULL.
 * Rets: Number of children.
 */
size_t pj_count(const pj_value *value)
{
    size_t n = 0;
    if (value != NULL)
        for (const pj_value *p = value->child; p != NULL; p = p->next)
            ++n;
    return n;
}

/* Purpose: Require exactly the named fields in an object.
 * Args: object and keys are borrowed; count is the key array length.
 * Rets: 1 valid, 0 invalid.
 */
int pj_fields(const pj_value *object, const char *const *keys, size_t count)
{
    if (object == NULL || object->type != PJ_OBJECT ||
        pj_count(object) != count)
        return 0;
    for (size_t i = 0; i < count; ++i)
        if (pj_get(object, keys[i]) == NULL)
            return 0;
    return 1;
}
