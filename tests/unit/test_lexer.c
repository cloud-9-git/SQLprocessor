#include "test_framework.h"

#include "sqlproc/diag.h"
#include "sqlproc/lexer.h"

static void test_lexer_tokenizes_keywords_and_literals(void) {
    TokenArray tokens;
    SqlError err;
    SqlStatus status = lexer_tokenize("INSERT INTO users VALUES (1, 'Alice', true);", &tokens, &err);

    ASSERT_STATUS_OK(status);
    ASSERT_INT_EQ(13, tokens.count);
    ASSERT_INT_EQ(TOKEN_INSERT, tokens.items[0].type);
    ASSERT_INT_EQ(TOKEN_IDENTIFIER, tokens.items[2].type);
    ASSERT_INT_EQ(TOKEN_NUMBER, tokens.items[5].type);
    ASSERT_INT_EQ(TOKEN_STRING, tokens.items[7].type);
    ASSERT_STR_EQ("Alice", tokens.items[7].lexeme);
    ASSERT_INT_EQ(TOKEN_TRUE, tokens.items[9].type);
    ASSERT_INT_EQ(TOKEN_EOF, tokens.items[12].type);
    token_array_free(&tokens);
}

static void test_lexer_tracks_string_escape_sequences(void) {
    TokenArray tokens;
    SqlError err;
    SqlStatus status = lexer_tokenize("SELECT 'A\\nB\\tC\\\\D';", &tokens, &err);

    ASSERT_STATUS_OK(status);
    ASSERT_STR_EQ("A\nB\tC\\D", tokens.items[1].lexeme);
    token_array_free(&tokens);
}

void register_lexer_tests(TestSuite *suite) {
    test_suite_add(suite, "lexer tokenizes keywords and literals", test_lexer_tokenizes_keywords_and_literals);
    test_suite_add(suite, "lexer decodes string escapes", test_lexer_tracks_string_escape_sequences);
}
