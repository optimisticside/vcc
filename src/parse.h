#ifndef _PARSE_H_
#define _PARSE_H_

/*
 * One allocated per parser.
 */
struct parser {
	struct token *token;	/* current token */
	struct tree *root;	/* root of syntax tree */
	struct parser *next;	/* next parser in list */
};

#endif /* !_PARSE_H_ */
