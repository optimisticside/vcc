#include "token.h"
#include "parse.h"
#include "tree.h"
#include "lex.h"

/*
 * Consume and return a token if matches current type. Otherwise, return null.
 */
static struct token *accept(struct parser *parser, int kind) {
	struct token *token;

	token = parser->token;
	if (token && token->kind == kind) {
		advance(parser);
		return token;
	}
	return NULL;
}

/*
 * Accept a token if valid. Otherwise throw an error.
 */
static struct token *expect(struct parser *parser, int kind) {
	struct token *token;

	token = parser->token;
	if ((token == NULL || token->kind != kind)
		fatalf("Expected %s, got %s", tokstr(kind), tokstr(token->kind));
	return token;
}

/*
 * Parse an if statement with a possible else case. Depend on the `if` token
 * to already be consumed.
 */
static struct tree *ifstmt(struct parser *parser) {
	struct tree *cond;
	struct tree *thenbody, *elsebody;

	expect(parser, T_LPAREN);
	cond = expr(parser);
	expect(parser, T_RPAREN);

	thenbody = stmt(parser);
	if (accept(parser, T_ELSE))
		elsebody = stmt(parser);

	return mkastnode(AST_IF, cond, thenbody, elsebody);
}

/*
 * Parse while loop. Depend on the `while` token to already be consumed.
 */
static struct tree *whilestmt(struct parser *parser) {
	struct tree *cond, *body;

	expect(parser, T_LPAREN);
	cond = expr(parser);
	expect(parser, T_RPAREN);
	body = stmt(parser);
	return mkastbinary(AST_WHILE, cond, body);
}

/*
 * Parse switch statement. Depend on the `switch` token to already be consumed.
 */
static struct tree *switchstmt(struct parser *parser) {
	struct tree *value, *body;

	expect(parser, T_LPAREN);
	value = expr(parser);
	expect(parser, T_RPAREN);
	body = stmt(parser);
	return mkastbinary(AST_SWITCH, value, body);
}

static struct tree *stmt(struct parser *parser) {

}
