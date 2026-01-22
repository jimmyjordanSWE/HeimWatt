/*
 * @file fuzz_http_parser.c
 * @brief AFL++ fuzz harness for HTTP parser
 *
 * Build: afl-gcc -o fuzz_http_parser fuzz_http_parser.c ../src/net/http_parse.c -I../include -I..
 * Run:   afl-fuzz -i corpus/http -o out/http -- ./fuzz_http_parser
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "net/http_parse.h"

// AFL persistence mode for faster fuzzing
#ifdef __AFL_HAVE_MANUAL_CONTROL
__AFL_FUZZ_INIT();
#endif

int main(void) {
#ifdef __AFL_HAVE_MANUAL_CONTROL
    __AFL_INIT();
    unsigned char *buf = __AFL_FUZZ_TESTCASE_BUF;

    while (__AFL_LOOP(10000)) {
        size_t len = __AFL_FUZZ_TESTCASE_LEN;
#else
    // Non-AFL mode: read from stdin
    char buf[8192];
    ssize_t n = read(STDIN_FILENO, buf, sizeof(buf) - 1);
    if (n <= 0)
        return 1;
    size_t len = (size_t) n;
    buf[len] = '\0';
#endif

        // Target function
        http_request req;
        int ret = http_parse_request((const char *) buf, len, &req);
        if (ret == 0) {
            http_request_destroy(&req);
        }

#ifdef __AFL_HAVE_MANUAL_CONTROL
    }
#endif

    return 0;
}
