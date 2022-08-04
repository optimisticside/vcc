#include "token.h"
#include "parse.h"
#include "tree.h"
#include "lex.h"

/*
 * Store tokens in sub-arrays based on their precedences, and a NULL at the end
 * of each sub array in the levels of tokens to indicate the end, rather than
 * storing a separate array of the subarray's lengths.
 */	
int *toklvls[] = {
	{ T_LOR, NULL },
	{ T_LAND, NULL },
	{ T_OR, NULL },
	{ T_XOR, NULL },
	{ T_AND, NULL },
	{ T_EQ, T_NE, NULL },
	{ T_LT, T_GT, T_LE, T_GE, NULL },
	{ T_LSHIFT, T_RSHIFT, NULL },
	{ T_PLUS, T_MINUS, NULL },
	{ T_STAR, T_SLASH, T_MOD, NULL },
};

/*
 * Tokens and their corresponding nodes in the abstract-syntax-tree. Stored in
 * an array since token-kinds are already integers.
 */
int tokmap[] = {
	[T_AND] = AST_AND,
}

/*
 * Tokens for assignments.
 */
int assigntoks[] = {
	T_MULASSIGN, T_DIVASSIGN, T_MODASSIGN, T_ADDASIGN, T_SUBASSIGN,
	T_LSHIFTASSIGN, T_RSHIFTASSIGN, T_ANDASSIGN, T_ORASSIGN, T_XORASSIGN,
}

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
		fatalf(
			"Expected %s, got %s",
			tokstr(kind),
			tokstr(token->kind)
		);
	return token;
}

/*
 * Gets the next token in the parser's internal token-queue.
 */
static struct token *peek(struct parser *parser) {
	return parser->token;
}

/*
 * Gets the nth token in the parser's internal token-queue, and NULL if not
 * existent.
 */
static struct token *peekn(struct parser *parser, int position) {
	struct token *token;

	token = parser->token;
	while (--position && token != NULL)
		token = token->next;
	return token;
}

/*
 * Parse a unary expression.
 *
 * unary-expression:
 *   postifix-expression
 *   ++ unary-expression
 *   -- unary-expression
 *   unary-operator cast-expression
 *   sizeof unary-expression
 *   sizeof ( unary-expression )
 *   alignof ( unary-expression )
 *
 * unary-operator:
 *   &
 *   *
 *   +
 *   -
 *   ~
 *   !
 *   ++
 *   --
 */
static struct tree *unaryexpr(struct parser *parser) {
	struct tree *child;
	bool hasparen;

	if (accept(parser, T_SIZEOF)) {
		hasparen = accept(parser, T_LPAREN) != NULL;
		child = unaryexpr(parser);
		if (hasparen)
			expect(parser, T_RPAREN);
		return mkastunary(AST_SIZEOF, child);
	}

	if (accept(parser, T_ALIGNOF)) {
		expect(parser, T_LPAREN);
		child = unaryexpr(parser);
		expect(parser, T_RPAREN);
		return mkastunary(AST_ALIGNOF, child);
	}
}

/*
 * Parse a type-cast expression.
 *
 * cast-expression:
 *   unary-expression
 *   ( type-name ) cast-expression
 *   ;
 */
static struct token *castexpr(struct parser *parser) {
	struct tree *right, *tn;

	if (peek(parser) == T_LPAREN && startstn(parser, peekn(parser, 2))) {
		tn = typename(parser);
	}
	right = unaryexpr(parser);
	if (tn != NULL)
		right = mkastbinary(AST_CAST, right, tn);
	return right;
}

/*
 * Internal routine to parse binary operators in expressions. Calls itself 
 * with a depth counter to measure precedence level.
 *
 * logical-or-expression:
 *   logical-and-expression
 *   logical-or-expression || logical-and-expression
 *   ;
 *
 * logical-and-expression:
 *   inclusive-and-expression
 *   logical-and-expression && inclusive-or-expression
 *   ;
 *
 * inclusive-or-expression:
 *   exclusive-or-expression
 *   inclusive-or-expression | exclusive-or-expression
 *   ;
 *
 * exclusive-or-expression:
 *   and-expression
 *   exclusive-or-expression ^ and-expression
 *   ;
 *
 * and-expression:
 *   equality-expression
 *   and-expression & equality-expression
 *   ;
 *
 * equality-expression:
 *   relational-expression
 *   equality-expression == relational-expression
 *   equality-expression != relational-expression
 *
 * relational-expression:
 *   shift-expression
 *   relational-expression < shift-expression
 *   relational-expression > shift-expression
 *   relational-expression <= shift-expression
 *   relational-expression >= shift-expression
 *   ;
 *
 * shift-expression:
 *   additive-expression
 *   shift-expression << additive-expression
 *   shift-expression >> additive-expression
 *
 * additive-expression:
 *   multiplicative-expression
 *   additive-expression + multiplicative-expression
 *   additive-expression - multiplicative-expression
 *   ;
 *
 * multiplicative-expression:
 *   cast-expression
 *   multiplicative-expression * cast-expression
 *   multiplicative-expression / cast-expression
 *   multiplicative-expression % cast-expression
 *   ;
 */
static struct tree *innerexpr(struct parser *parser, int level) {
	struct token *token;
	struct tree *left;
	int t;

	if (level >= sizeof(levels))
		return castexpr(parser);
	left = innerexpr(level + 1);
	if (peek(parser, T_EOF))
		return left;
	for (;;) {
		token = NULL;
		for (t = &toklvls[level][0]; t != NULL; t++) {
			if ((token = accept(*t)) != NULL)
				break;
		}
		if (token == NULL || token->kind == T_EOF)
			break;
		left = mkastbinary(tokmap[token->kind], left, innerexpr(level + 1));
	}
	return left;
}

/*
 * Parse a conditional expression.
 *
 * conditional-expression:
 *   logical-or-expression
 *   logical-or-expression ? expression : conditional-expression
 *   ;
 */
static struct token *condexpr(struct parser *parser) {
	struct tree *left, *truexpr;

	left = innerexpr(0);
	if (accept(T_QUESTIONMARK)) {
		truexpr = expr(parser);
		expect(T_COLON);
		left = mkastnode(AST_COND, left, truexpr, condexpr(parser));
	}
	return left;
}

/*
 * Parse an assignment expression.
 *
 * assignment-expression:
 *   conditional-expression
 *   unary-expression assignment-operator assignment-expression
 */
static struct token *assignexpr(struct parser *parser) {
	struct token *token;
	struct tree *left;
	int t;

	/*
	 * TODO: This is the same code that is used in the `innerexpr`
	 * subroutine, which is a code smell.
	 */
	left = unaryexpr(parser);
	for (t = &assignopers[0]; t < &assignopers[sizeof(assignopers)]; t++) {
		if ((token = accept(*t)) != NULL)
			break;
	}
	if (token == NULL || token->kind == T_EOF)
		return left;
	return mkastbinary(tokmap[token->kind], left, assignexpr(parser));
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
