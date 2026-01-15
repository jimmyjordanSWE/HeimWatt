/**
 * @file fuzz_json_parser.c
 * @brief AFL++ fuzz harness for JSON parser
 *
 * Build: afl-gcc -o fuzz_json_parser fuzz_json_parser.c ../libs/cJSON.c ../src/net/json.c
 * -I../include -I.. Run:   afl-fuzz -i corpus/json -o out/json -- ./fuzz_json_parser
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "net/json.h"

#ifdef __AFL_HAVE_MANUAL_CONTROL
__AFL_FUZZ_INIT();
#endif

int main(void)
{
#ifdef __AFL_HAVE_MANUAL_CONTROL
    __AFL_INIT();
    unsigned char *buf = __AFL_FUZZ_TESTCASE_BUF;

    while (__AFL_LOOP(10000))
    {
        size_t len = __AFL_FUZZ_TESTCASE_LEN;

        // Null-terminate for JSON parsing
        char *input = malloc(len + 1);
        if (!input) continue;
        memcpy(input, buf, len);
        input[len] = '\0';

        // Target function
        json_value *v = json_parse(input);
        if (v)
        {
            char *str = json_stringify(v);
            free(str);
            json_free(v);
        }

        free(input);
    }
#else
    char buf[8192];
    ssize_t n = read(STDIN_FILENO, buf, sizeof(buf) - 1);
    if (n <= 0) return 1;
    size_t len = (size_t) n;

    char *input = malloc(len + 1);
    if (!input) return 1;
    memcpy(input, buf, len);
    input[len] = '\0';

    json_value *v = json_parse(input);
    if (v)
    {
        char *str = json_stringify(v);
        free(str);
        json_free(v);
    }

    free(input);
#endif

    return 0;
}
