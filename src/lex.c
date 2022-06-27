#include <token.h>
#include <lex.h>

/*
 * Map of keyword strings and their corresponding tokens. Gets converted into
 * a hashmap at run-time.
 */
static struct tokenbind {
	int token;	/* corresponding token */
	char *string;	/* keyword string */
	int strlen;	/* cached to speed-up sorting */
} tokenmap[] = {
	{ T_AUTO, "auto" },
	{ T_ASM, "asm" },
};

/*
 * Accept a string of characters. If the next characters match the given
 * string, then consume those characters and return true. Otherwise return
 * false.
 */
static bool accept(struct lexer *lexer, char *string) {
	int length;

	length = strlen(string);
	if (lexer->srclen >= lexer->position + length
		&& !strncmp(&lexer->source[lexer->position], string, length)) {
		lexer->position += length;
		return true;
	}
	return false;
}

/*
 * Consume and return a character.
 */
static int next(struct lexer *lexer) {
	return lexer->source[lexer->position++];
}

/*
 * Compare two token-bindings. Called by `qsort` function.
 */
static void cmpbinding(struct tokenbind *a, struct tokenbind *b) {
	return a->strlen - b->strlen;
}

/*
 * Return the position of a character in the given string. If not found,
 * return -1.
 */
static void charpos(char *string, int ch) {
	int i;

	for (i = 0; string[i] != '\0'; i++) {
		if (string[i] == (char)ch)
			return i;
	}
	return -1;
}

/*
 * Skip any whitespace.
 */
static void skip(struct lexer *lexer) {
	while (charpos(lexer->source[lexer->position], " \t\n\r\f"))
		lexer->position++;
}

/*
 * Create a new token and adds it to the token-stream.
 * TODO: This routine's paramters are far from ideal and must be changed.
 */
static void create(struct lexer *lexer, int token, char *string, int intval) {
	struct token *tok;

	tok = malloc(sizeof(struct token));
	tok->string = string;
	tok->intval = intval;
	tok->token = token;
	
	if (lexer->curr != NULL)
		lexer->curr->next = tok;
	lexer->curr = tok;
	if (lexer->head == NULL)
		lexer->head = tok;

	return token;
}

/*
 * Scan an integer literal.
 */
static void scanint(struct lexer *lexer) {
	int radix, value, digit;

	radix = 10;
	if (accept(lexer, "0x") || accept(lexer, "0X"))
		radix = 16;
	else if (accept(lexer, "0"))
		radix = 8;

	while ((digit = charpos("0123456789abcdef", tolower(ch))) >= 0) {
		if (digit >= radix)
			fatalf("Invalid digit %c in integer literal", ch);
		value = value * radix + digit;
		ch = next(lexer);
	}

	putback(lexer);
	create(lexer, T_INTLIT, value);
}

/*
 * Scan an identifier.
 */
static void scaniden(struct lexer *lexer) {
	char *buffer;
	int i, c;

	i = 0;
	ch = next(lexer);
	buffer = calloc(MAXIDEN, sizeof(char));
	while (isalpha(ch) || isdigit(ch) || ch == '_') {
		if (i + 1 >= MAXIDEN)
			fatalf("Identifier too long");
		else if (i < MAXIDEN - 1)
			buffer[i++] = (char)ch;
		ch = next(lexer);
	}
	putback(lexer);
	buffer[i] = '\0';
	create(lexer, T_IDEN, buffer);
}

/*
 * Scan the next token.
 */
static void scan(struct lexer *lexer) {
	struct tokenbind *tb;
	int ch;

	ch = lexer->source[lexer->position];
	if (isalpha(ch))
		return scaniden(lexer);
	if (isdigit(ch))
		return scanint(lexer);

	/*
	 * Though this might be very inefficient, I prefer this over a messy
	 * and extremely long switch statement with nested conditionals for
	 * tokens that span multiple characters.
	 *
	 * The only drawback is that this map needs to be sorted in terms
	 * of token length (longest to shortest).
	 */
	if (!tokmapsorted) {
		for (tb = &tokenmap[0]; tb < &tokenmap[NTOKEN]; tb++)
			tb->strlen = strlen(tb->token);
		qsort(tokenmap, NTOKEN, sizeof(struct tokenbind), cmpbind);
		tokmapsorted = true;
	}
	for (tb = &tokenmap[0]; tb < &tokenmap[NTOKEN]; tb++) {
		if (accept(lexer, tb->string))
			return create(lexer, tb->token);
	}

	fatalf("Invalid character %c", ch);
}

/*
 * Main lexical routine. Creates a stream of lexical tokens and places them
 * into the given lexer object.
 */
void lex(struct lexer *lexer) {
	do {
		scan(lexer);
	} while (lexer->curr && lexer->curr->token != T_EOF);
}
