#include "token.h"
#include "lex.h"

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
static void create(struct lexer *lexer, int token, long value) {
	struct token *tok;

	tok = malloc(sizeof(struct token));
	tok->value = value;
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
	int size, ch;

	size = 0;
	ch = next(lexer);
	buffer = calloc(MAXIDEN, sizeof(char));
	while (isalpha(ch) || isdigit(ch) || ch == '_') {
		if (i + 1 >= MAXIDEN)
			fatalf("Identifier too long");
		else if (size <= MAXIDEN)
			buffer[size++] = (char)ch;
		ch = next(lexer);
	}
	putback(lexer);
	buffer[size] = '\0';
	return create(lexer, T_IDEN, buffer);
}

/*
 * Scan a character literal.
 */
static void scanchar(struct lexer *lexer) {
	long value;
	int length, ch;

	value = 0;
	while ((ch = next(lexer)) != '\'') {
		if (length++ > sizeof(long))
			fatalf("Character-literal too long");
		value = (value << 8) | (ch & 0xFF);
	}
	return create(lexer, T_CHARLIT, value);
}

/*
 * Scan a string literal.
 */
static void scanstr(struct lexer *lexer) {
	char *buffer;
	int size, capacity, ch;

	size = 1;
	capacity = 1;
	buffer = calloc(1, sizeof(char));
	while ((ch = next(lexer)) != '"') {
		/*
		 * We do not do not follow the traditional doubleing method
		 * for dynamic arrays because it can be quite inefficient
		 * for our use case. We instead increase the buffer by a
		 * constant amount.
		 */
		if (size + 1 >= capacity) {
			capacity += BUFFER_DELTA;
			buffer = realloc(buffer, capacity);
		}
		buffer[size++] = ch;
	}
	buffer[size] = '\0';
	return create(lexer, T_STRLIT, buffer);
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

	if (ch == '"')
		return scanstr(lexer);
	if (ch == '\'')
		return scanchar(lexer);

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
