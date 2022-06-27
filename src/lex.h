#ifndef _LEX_H_
#define _LEX_H_

/*
 * One allocated per lexer.
 */
struct lexer {
	char *source;		/* content to lex */
	int position;		/* position in source */
	struct token *head;	/* head token */
	struct token *curr;	/* current token */
	struct lexer *next;	/* next lexer in list */
};

#endif /* !_LEX_H_ */
