#include "munit/munit.h"

#include <stdio.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "base64.h"
#include "log.h"
#include "serializer.h"
#include "variable.h"
#include "test_common.h"

#define TEST_FILE "data/legacy-payload-b64.txt"

#define BASE64_MAX (4096*8)
#define BUFFER_MAX (4096*8)
#define SERIALIZED_MAX (4096*8)
#define VAR_MAX 512

void test_base64(void)
{
    int ret;
    uint8_t buffer[BUFFER_MAX] = {0};
    char base64[BASE64_MAX] = {0};
    uint8_t serialized[SERIALIZED_MAX] = {0};
    uint8_t *ptr;
    variable_t vars[VAR_MAX];
    FILE *fd;

    memset(vars, 0, sizeof(vars));

    fd = fopen(TEST_FILE, "r");
    munit_assert(fd != NULL);

    fread(base64, 1, BASE64_MAX, fd);
    fclose(fd);

    ret = base64_to_bytes(buffer, BUFFER_MAX, base64, BASE64_MAX);
    munit_assert(ret >= 0);

    ret = from_bytes_to_vars(vars, VAR_MAX, buffer, (size_t)ret);
    munit_assert(ret >= 0);

    DDEBUG("Variables(%d)\n", ret);
    dprint_variable_list(vars, ret);

    ptr = serialized;

    ret = serialize_variable_list(&ptr, SERIALIZED_MAX, vars, VAR_MAX);
    munit_assert(ret >= 0);

    fd = fopen("tmp.data", "w");
    munit_assert(fd != NULL);

    fwrite(serialized, 1, SERIALIZED_MAX, fd);
    fclose(fd);

    munit_assert(memcmp(serialized, buffer, min(SERIALIZED_MAX, BUFFER_MAX)) == 0);

    return;
}
