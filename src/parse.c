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
	if (token == NULL || token->kind != kind)
		/*
		 * TODO: Can we realy rely on the fact that the last token will
		 * be an EOF token when access the current token's kind (it can
		 * still be NULL)
		 */
		fatalf("Expected %s, got %s", tokstr(kind), tokstr(token->kind));
	return token;
}

/*
 * Parse an assignment expression.
 *
 * assignment-expression:
 *   conditional-expression
 *   unary-expression assignment-operator assignment-expression
 */
static struct token *assmtexpr(struct parser *parser) {
	static int assignopers[] = {
		T_MULASSIGN, T_DIVASSIGN, T_MODASSIGN, T_ADDASIGN, T_SUBASSIGN,
		T_LSHIFTASSIGN, T_RSHIFTASSIGN, T_ANDASSIGN, T_ORASSIGN,
		T_XORASSIGN,
	};


}

/*
 * Parse an expression.
 *
 * expression:
 *   assignment-expression
 *   expression, assignment-expression
 *   ;
 */
static struct token *expr(struct parser *parser) {
	struct tree *left;

	left = assignmentexpr(parser);
	while (accept(parser, T_COMMA)) {
		left = mkastbinary(
			AST_COMPOUNDEXPR,
			left,
			assignmentexpr(parser)
		);
	}
	return left;
}

/*
 * Parse a direct-declarator.
 *
 * direct-declarator:
 *   identifier
 *   ( declarator )
 *   direct-declarator [ ]
 *   direct-declarator [ number ]
 *   direct-declarator [ static type-qualifier-list assignment-expression ]
 *   direct-declarator [ static assignment-expression ]
 *   direct-declarator [ type-qualifier-list number ]
 *   direct-declarator [ type-qualifier-list static assignment-expression ]
 *   direct-declarator [ type-qualifier-list ]
 *   direct-declarator [ assignment-expression ]
 *   direct-declarator ( parameter-type-list )
 *   direct-declarator ( )
 *   direct-declarator ( identifier-list )
 */
static struct tree *directdeclarator(struct parser *parser) {
	if (peek(parser, T_NAME)) {
		
	}

	if (accept(parser, T_LPAREN)) {
		struct declarator *inner = declarator(parser);
		expect(parser, T_RPAREN);
	}
}

/*
 * Parse a declarator.
 *
 * declarator:
 *   pointer direct-declarator
 *   direct-declarator
 *
 */
static struct declarator *declarator(struct parser *parser) {
	if (accept(parser, T_STAR)) {
		struct declarator *inner;
		if ((inner = declarator(parser)) != NULL)
			return mkptrdeclarator(inner);
	}
	return directdeclarator(parser);
}

/*
 * Parse a declaration.
 *
 * declaration:
 *   declaration-specifiers ;
 *   declaration-specifiers init-declarator-list ;
 *   static_assert-declaration
 *   ;
 *
 * declaration-list:
 *   declaration
 *   declaration-list declaration
 */
static struct tree *declaration(struct parser *parser) {
	struct tree *toassert, *errmsg;

	if (accept(parser, T_STATICASSERT)) {
		expect(parser, T_LPAREN);
		toassert = constexpr(parser);

		expect(parser, T_COMMA);
		errmsg = expect(parser, T_STRING);

		expect(parser, T_RPAREN);
		expect(parser, T_SEMI);
		return mkastbinary(AST_STATICASSERT, toassert, errmsg);
	}
}

/*
 * Parse an if statement with a possible else case.
 *
 * if-statement:
 *   if ( expression ) statement
 *   if ( expression ) statement else statement
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
 * while-statement:
 *   while ( expression ) statement
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
 * do-statement:
 *   do statement while ( expression ) ;
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
 * Parse a switch statement.
 *
 * switch-statement:
 *   switch ( expression ) statement
 */
static struct tree *switchstmt(sturct parser *parser) {
	struct tree *value, body;

	expect(parser, T_SWITCH);
	expect(parser, T_LPAREN);
	value = expr(parser);

	expect(parser, T_RPAREN);
	body = stmt(parser);

	return mkastbinary(AST_SWITCHSTMT, value, body);
}

/*
 * Parse a satement without labels. Called by the main statement parser after
 * consuming any labels.
 */
static struct tree *stmtnolables(struct parser *parser) {
	switch (parser->token->kind) {
	case T_LBRACE:
		return compoundstmt(parser);

	/*
	 * Iteration statements.
	 */
	case T_WHILE:
		return whilestmt(parser);
	case T_DO:
		return dostmt(parser);
	case T_FOR:
		return forstmt(parser);

	/*
	 * Selection statement.
	 */
	case T_IF:
		return ifstmt(parser);
	case T_SWITCH:
		return stwitchstmt(parser);
	
	/*
	 * Jump statement.
	 */
	case T_GOTO:
		return gotostmt(parser);
	case T_CONTINUE:
		return contstmt(parser);
	case T_BREAK:
		return breakstmt(parser);
	case T_RETURN:
		return returnstmt(parser);
	}
}

/*
 * Parse a labeled statement.
 *
 * labeled-statement:
 *   identifier : statement
 *   case constant-expression : statement
 *   default : statement
 *   ;
 */
static struct tree *labeledstmt(struct parser *parser) {
	struct tree *label, *caseval, body;

	if (accept(parser, T_CASE)) {
		caseval = constexpr(parser);
		expect(parser, T_COLON);
		return mkastbinary(AST_CASE, caseval, stmt(parser));
	}

	if (accept(parser, T_DEFAULT)) {
		expect(parser, T_COLON);
		return mkastunary(AST_DEFAULTCASE, stmt(parser));
	}

	if ((label = accept(parser, T_NAME)) != NULL) {
		expect(parser, T_COLON);
		return mkastbinary(AST_LABEL, label, stmt(parser));
	}
}

/*
 * Parse a statement.
 *
 * statement:
 *   labeled-statement
 *   compound-statement
 *   expression-statement
 *   selection-statement
 *   iteration-statement
 *   jump-statement
 *   ;
 *
 * iteration-statement:
 *   while ( expression ) statement
 *   do statement while ( expression ) ;
 *   for ( expression-statement expression-statement ) statement
 *   for ( expression-statement expression-statement expression-statement ) statement
 *   for ( declaration expression-statement ) statement
 *   for ( declaration expression-statement expression-statement ) statement
 *
 * selection-statement:
 *   if ( expression ) statement
 *   if ( expression ) statement else statement
 *   switch ( expression ) statement
 * 
 * jump-statement:
 *   goto identifier ;
 *   continue ;
 *   break ;
 *   return ;
 *   return expression ;
 */
static struct tree *stmt(struct parser *parser) {
	labels(parser);
}
