#include "pkg_json.h" /* pj_parse, pj_free. */
#include <stdio.h>    /* fread, stdin. */
/* Purpose: Test bounded JSON parsing with arbitrary process input bytes.
 * Args: none.
 * Rets: 0 for valid JSON, 1 for invalid JSON.
 */
int main(void)
{
    char data[262145];
    size_t count = fread(data, 1, sizeof(data), stdin);
    pj_value *tree = pj_parse(data, count);
    int status = tree == NULL;
    pj_free(tree);
    return status;
}
