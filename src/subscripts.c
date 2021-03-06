#define _GNU_SOURCE

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "footprints.h"

const char *subscript_methods_str[] = {
	"SUBSCRIPT_DIRECT_BYTES",
	"SUBSCRIPT_DEREF_BYTES",
	"SUBSCRIPT_DEREF_SIZES"
};

extern struct uniqtype __uniqtype__unsigned_char$8;
static struct uniqtype *byte_type = &__uniqtype__unsigned_char$8;

struct union_node *construct_bytes_union(struct object obj, size_t base, size_t length, enum footprint_direction direction) {
	struct union_node *tail = NULL;
	size_t orig_addr = (size_t) obj.addr + base;
	for (size_t ptr = orig_addr; ptr < (orig_addr + length); ptr++) {
		struct expr *new_byte = expr_new_with(direction, EXPR_OBJECT);
		new_byte->object.type = byte_type;
		new_byte->object.addr = (void*)ptr;
		new_byte->object.direct = false;
		struct union_node *head = union_new_with(new_byte, true, tail);
		tail = head;
	}
	return tail;
}

// TODO: bounds checking?
struct union_node *construct_size_union(struct object obj, size_t base, size_t length, enum footprint_direction direction) {
	assert(UNIQTYPE_HAS_KNOWN_LENGTH(obj.type));
	struct union_node *tail = NULL;
	// TODO FIXME HACK to deal with void*
	// (not *so* bad because of the assertion above)
	size_t size = (obj.type->pos_maxoff == 0 ? 1 : obj.type->pos_maxoff);
	size_t orig_addr = (size_t) obj.addr + (size * base);
	for (size_t ptr = orig_addr; ptr < (orig_addr + size * length); ptr += size) {
		struct expr *new_obj = expr_new_with(direction, EXPR_OBJECT);
		new_obj->object.type = obj.type;
		new_obj->object.addr = (void*)ptr;
		new_obj->object.direct = false;
		struct union_node *head = union_new_with(new_obj, true, tail);
		tail = head;
	}
	return tail;
}

struct union_node *bytes_union_from_object(struct object obj, enum footprint_direction direction) {
	assert(UNIQTYPE_HAS_KNOWN_LENGTH(obj.type));
	size_t size = obj.type->pos_maxoff;
	return construct_bytes_union(obj, 0, size, direction);
}


struct expr *eval_subscript(struct evaluator_state *state, struct expr *e, struct env_node *env) {
	assert(e->type == EXPR_SUBSCRIPT);
	struct expr *target_expr = eval_footprint_expr(state, e->subscript.target, env);
	if (target_expr->type == EXPR_UNION) {
		char *loop_var_name = new_ident_not_in(env, "loop_var");

		struct expr *loop_var_ident = expr_new_with(e->direction, EXPR_IDENT);
		loop_var_ident->ident = loop_var_name;

		struct expr *loop_body = expr_new();
		memcpy(loop_body, e, sizeof(struct expr));
		loop_body->subscript.target = loop_var_ident;

		struct expr *loop = expr_new_with(e->direction, EXPR_FOR);
		loop->for_loop.body = loop_body;
		loop->for_loop.ident = loop_var_name;
		loop->for_loop.over = target_expr;

		return eval_footprint_expr(state, loop, env);
	} else if (target_expr->type == EXPR_OBJECT) {
		struct object target = target_expr->object;
		int64_t from, to, length;
		_Bool from_success, to_success;
		struct expr *partial_from, *partial_to;
		from_success = eval_to_value(state, e->subscript.from, env, &partial_from, &from);
		if (e->subscript.to) {
			to_success = eval_to_value(state, e->subscript.to, env, &partial_to, &to);
		} else {
			to_success = false;
		}

		if (!from_success || (e->subscript.to && !to_success)) {
			// cache miss, state modified
			struct expr *new_expr = expr_clone(e);
			if (from_success) {
				new_expr->subscript.from = construct_value(from, e->direction);
			} else {
				new_expr->subscript.from = partial_from;
			}
			if (e->subscript.to) {
				if (to_success) {
					new_expr->subscript.to = construct_value(to, e->direction);
				} else {
					new_expr->subscript.to = partial_to;
				}
			}
			return new_expr;
		}
		
		struct object derefed;
		if (e->subscript.to) {
			if (to == from) {
				return construct_void();
			}
			assert(to > from);
			length = to - from;
		} else {
			length = -1;
		}
		switch (e->subscript.method) {
		case SUBSCRIPT_DIRECT_BYTES: {
			if (e->subscript.to) {
				//return construct_union(construct_bytes_union(target, from, length));
				return construct_extent((unsigned long) target.addr + from, length, e->direction);
			} else {
				struct object new_obj;
				new_obj.type = byte_type;
				new_obj.addr = (void*)((unsigned long) target.addr + from);
				new_obj.direct = false;
				return construct_object(new_obj, e->direction);
			}
		} break;
		case SUBSCRIPT_DEREF_BYTES: {
			struct object derefed;
			if (!deref_object(state, target, &derefed)) {
				// cache miss, state modified
				return e;
			}
			if (e->subscript.to) {
				//return construct_union(construct_bytes_union(derefed, from, length));
				return construct_extent((unsigned long) derefed.addr + from, length, e->direction);
			} else {
				struct object new_obj;
				new_obj.type = byte_type;
				new_obj.addr = (void*)((unsigned long) derefed.addr + from);
				new_obj.direct = false;
				return construct_object(new_obj, e->direction);
			}
		} break;
		case SUBSCRIPT_DEREF_SIZES: {
			struct object derefed;
			if (!deref_object(state, target, &derefed)) {
				// cache miss, state modified
				return e;
			}
			assert(UNIQTYPE_HAS_KNOWN_LENGTH(derefed.type));
			if (e->subscript.to) {
				assert(length > 0);
				return construct_union(construct_size_union(derefed, from, length, e->direction), e->direction);
			} else {
				struct object new_obj = derefed;
				size_t size = derefed.type->pos_maxoff;
				new_obj.addr = (void*) ((size_t)derefed.addr + (from * size));
				return construct_object(new_obj, e->direction);
			}
		} break;
		default:
			assert(false);
			break;
		}
	} else {
		assert(false);
	}
}
