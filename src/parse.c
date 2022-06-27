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
		/*
		 * TODO: Can we realy rely on the fact that the last token will
		 * be an EOF token when access the current token's kind (it can
		 * still be NULL)
		 */
		fatalf("Expected %s, got %s", tokstr(kind), tokstr(token->kind));
	return token;
}

/*
 * Parse an if statement with a possible else case.
 *
 * If statement:
 * if ( expression ) statement
 * if ( expression ) statement else statement
 */
static struct tree *ifstmt(struct parser *parser) {
	struct tree *cond;
	struct tree *thenbody, *elsebody;

	expect(parser, T_IF);
	expect(parser, T_LPAREN);
	cond = expr(parser);
	expect(parser, T_RPAREN);

	thenbody = stmt(parser);
	if (accept(parser, T_ELSE))
		elsebody = stmt(parser);
	return mkastnode(AST_IFSTMT, cond, thenbody, elsebody);
}

/*
 * Parse a while statement.
 *
 * While statement:
 * while ( expression ) statement
 */
static struct tree *whilestmt(struct parser *parser) {
	struct tree *cond, *body;

	expect(parser, T_WHILE);
	expect(parser, T_LPAREN);
	cond = expr(parser);
	expect(parser, T_RPAREN);

	body = stmt(parser);
	return mkastbinary(AST_WHILESTMT, cond, body);
}

/*
 * Parse a do statement.
 *
 * Do statement:
 * do statement while ( expression ) ;
 */
static struct tree *dostmt(struct parser *parser) {
	struct tree *body, *cond;

	expect(parser, T_DO);
	body = stmt(parser);

	expect(parser, T_WHILE);
	expect(parser, T_LPAREN);
	cond = expr(parser);
	expect(parser, T_RPAREN);
	expect(parser, T_SEMI);

	return mkastbinary(AST_DOSTMT, cond, body);
}

/*
 * Parse a statement.
 */
static struct tree *stmt(struct parser *parser) {
	switch (parser->token->kind) {
	case T_IF:
		return ifstmt();
	case T_WHILE:
		return whilestmt();
	}
}
