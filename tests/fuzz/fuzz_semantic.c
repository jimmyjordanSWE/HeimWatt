/**
 * @file fuzz_semantic.c
 * @brief AFL++ fuzz harness for semantic type parsing
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "semantic_types.h"

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
#else
    char buf[1024];
    ssize_t n = read(STDIN_FILENO, buf, sizeof(buf) - 1);
    if (n <= 0) return 0;
    size_t len = (size_t) n;
#endif

        // Null terminate input
        char *input = malloc(len + 1);
        if (!input)
        {
#ifdef __AFL_HAVE_MANUAL_CONTROL
            continue;
#else
        return 0;
#endif
        }
        memcpy(input, buf, len);
        input[len] = 0;

        // Target function
        semantic_type type = semantic_from_string(input);
        (void) type;  // Use result to prevent optimization

        free(input);

#ifdef __AFL_HAVE_MANUAL_CONTROL
    }
#endif

    return 0;
}
