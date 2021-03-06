#define _GNU_SOURCE

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <antlr3interfaces.h>
#include <antlr3tokenstream.h>
#include <antlr3commontree.h>
#include "dwarfidl/dwarfidlNewCParser.h"
#include "dwarfidl/dwarfidlNewCLexer.h"
#include "footprints.h"



typedef ANTLR3_TOKEN_SOURCE TokenSource;
typedef ANTLR3_COMMON_TOKEN CommonToken;
typedef ANTLR3_INPUT_STREAM ANTLRInputStream;
typedef ANTLR3_COMMON_TOKEN_STREAM CommonTokenStream;
typedef ANTLR3_BASE_TREE Tree;
typedef ANTLR3_COMMON_TREE CommonTree;

int main(int argc, char **argv) {
	assert(argc == 2);
	const char *filename = argv[1];
	
	pANTLR3_INPUT_STREAM in_fileobj = antlr3FileStreamNew((uint8_t *) filename,
	                                                      ANTLR3_ENC_UTF8);
	dwarfidlNewCLexer *lexer = dwarfidlNewCLexerNew(in_fileobj);
	CommonTokenStream *tokenStream =
		antlr3CommonTokenStreamSourceNew(ANTLR3_SIZE_HINT, TOKENSOURCE(lexer));
	dwarfidlNewCParser *parser = dwarfidlNewCParserNew(tokenStream); 
	
	dwarfidlNewCParser_expression_return ret = parser->expression(parser);
	Tree *tree = ret.tree;
	
	pANTLR3_STRING s = tree->toStringTree(tree);
	
	int test_int = 42;
	int *test_int_ptr = &test_int;
	struct object test_int_object = {&__uniqtype__int$32, &test_int, false};
	struct object test_int_ptr_object = {&__uniqtype____PTR_int$32, &test_int_ptr, false};
	struct env_node *env = env_new_with("test_int_ptr",
	                                    construct_object(test_int_ptr_object, FP_DIRECTION_UNKNOWN),
	                                    env_new_with("test_int",
	                                                 construct_object(test_int_object, FP_DIRECTION_UNKNOWN), NULL));

	printf("antlr: %s\n", (char*) s->chars);
	struct expr *parsed = parse_antlr_tree(tree, FP_DIRECTION_UNKNOWN);
	printf("parsed: %s\n", print_expr_tree(parsed));

	struct data_extent_node fake_extent = {
		(struct data_extent) {
			(size_t) &test_int, sizeof(test_int), &test_int
		},
		NULL
	};
	
	struct evaluator_state state = {
		parsed,
		env,
		NULL,
		&fake_extent,
		NULL,
		false
	};
	
	struct expr *evaled = eval_footprint_expr(&state, parsed, env);

	printf("evaled: %s\n", print_expr_tree(evaled));

	if (evaled->type == EXPR_UNION) {
		evaled->unioned = union_flatten(evaled->unioned);
		evaled->unioned = union_objects_to_extents(evaled->unioned);
		union_sort(&evaled->unioned);
		evaled->unioned = sorted_union_merge_extents(evaled->unioned);
		printf("flattened and sorted and merged: %s\n", print_expr_tree(evaled));
	}

	printf("test_int: %d. test_int_ptr: %p\n", test_int, test_int_ptr);

	return 0;
}

