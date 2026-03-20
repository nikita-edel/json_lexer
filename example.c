#include "json_lexer.h"
#include <stdio.h>

const char json[] =
        "[{"
                "\"name\": \"Jeff\","
                "\"age\": 30,"
                "\"active\": true,"
                "\"scores\": [1, 2, 3],"
                "\"address\": null"
        "}]";

int main(void) {

    json_lexer_t lexer;
    json_lexer_init(&lexer, json, json + sizeof json - 1);

    while (json_lexer_next_token(&lexer) == JSON_LEX_OK) {
        printf("token: %s, len: %zu\n", JSON_TOKEN_STR(lexer.token.type), (size_t) lexer.token.strLen);
    }

    if (lexer.last_status != JSON_LEX_EOF) {
        printf("Error occured: %s\n", JSON_LEX_ERR_STR(lexer.last_status));
    }

    return 0;
}
