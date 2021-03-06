#define _GNU_SOURCE

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "footprints.h"

////////////////////////////////////////////////////////////
// struct env_node
////////////////////////////////////////////////////////////

struct env_node *env_new() {
	struct env_node *result = malloc(sizeof(struct env_node));
	memset(result, 0, sizeof(struct env_node));
	return result;
}

struct env_node *env_new_with(char *name, struct expr *expr, struct env_node *next) {
	struct env_node *result = env_new();
	result->name = name;
	result->expr = expr;
	result->next = next;
	return result;
}

void env_free_node(struct env_node **current) {
	free(*current);
	*current = NULL;
}


void env_free(struct env_node *first) {
	struct env_node *current = first;
	struct env_node *next;
	while (current != NULL) {
		next = current->next;
		env_free_node(&current);
		current = next;
	}
}


struct expr *lookup_in_object(struct object *context, char *ident, enum footprint_direction direction) {
	assert(context != NULL);
	assert(context->type != NULL);
	struct object obj;
	size_t i;
	if (UNIQTYPE_IS_COMPOSITE_TYPE(context->type)) {
		for (i = 0; i < UNIQTYPE_COMPOSITE_MEMBER_COUNT(context->type); i++) {
			if (strcmp(ident, UNIQTYPE_COMPOSITE_SUBOBJ_NAMES(context->type)[i]) == 0) {
				obj.type = context->type->related[i].un.memb.ptr;
				obj.addr = (void*) context->addr + context->type->related[i].un.memb.off;
				obj.direct = false;
				return construct_object(obj, direction);
			}
		}
	}

	// not found
	assert(false);
}

struct expr *lookup_in_env(struct env_node *env, char *ident) {
	struct env_node *current = env;
	while (current != NULL) {
		if (strcmp(ident, current->name) == 0) {
//			fpdebug(state, "looked up %s and found an %s", ident, expr_types_str[current->expr->type]);
			if (current->expr->type == EXPR_VALUE) {
//				fpdebug(state, " with value %ld", current->expr->value);
			}
//			fpdebug(state, "\n");
//			   fpdebug(state, "looked up %s and found %p\n", ident, (void*) object_to_value(state, current->value));
			return current->expr;
		}
		current = current->next;
	}

	// not found
	assert(false);
}

struct expr *eval_ident(struct evaluator_state *state, struct expr *e, struct env_node *env) {
	assert(e->type == EXPR_IDENT);
	return lookup_in_env(env, e->ident);
}

char *new_ident_not_in(struct env_node *env, char *suffix) {
	size_t length = strlen(suffix);
	char *copy = malloc(length+1); // + \0
	strcpy(copy, suffix);
	return _find_ident_not_in(env, copy, length);
}

char *_find_ident_not_in(struct env_node *env, char *suffix, size_t length) {
	if (!_ident_in(env, suffix)) {
		return suffix;
	} else {
		// copy = _suffix
		char *copy = malloc(length+2); // + 1 + \0
		copy[0] = '_';
		strcpy(copy+1, suffix);
		// we own suffix because this is only called behind new_ident_not_in
		free(suffix);
		return _find_ident_not_in(env, copy, length+1);
	}
}

_Bool _ident_in(struct env_node *env, char *ident) {
	struct env_node *current = env;
	while (current != NULL) {
		if (current->name == ident) {
			return true;
		}
		current = current->next;
	}
	return false;
}

char *eval_to_ident(struct evaluator_state *state, struct expr *e, struct env_node *env) {
	struct expr *result = eval_footprint_expr(state, e, env);
	assert(result->type == EXPR_IDENT);
	return result->ident;
}
