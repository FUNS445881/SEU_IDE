/* 由 SeuYacc 生成的 LR(1) 解析器 */

#include "minic.tab.h"
#include <stdio.h> /* 包含标准输入输出库，因为使用了printf */
#include <stdlib.h>
#include <string.h>

/* 错误收集功能 */
typedef struct ErrorInfo {
    int line;
    char* message;
    char* actual_token;
    char** expected_tokens;
    int expected_count;
} ErrorInfo;

static ErrorInfo* errors = NULL;
static int error_count = 0;
static int error_capacity = 0;

static void add_error(int line, const char* msg, const char* actual, 
                     const char** expected, int exp_count) {
    if (error_count >= error_capacity) {
        error_capacity = error_capacity == 0 ? 10 : error_capacity * 2;
        errors = (ErrorInfo*)realloc(errors, error_capacity * sizeof(ErrorInfo));
    }
    ErrorInfo* err = &errors[error_count];
    err->line = line;
    err->message = strdup(msg);
    err->actual_token = actual ? strdup(actual) : NULL;
    err->expected_count = exp_count;
    if (exp_count > 0 && expected) {
        err->expected_tokens = (char**)malloc(exp_count * sizeof(char*));
        for (int i = 0; i < exp_count; i++) {
            err->expected_tokens[i] = strdup(expected[i]);
        }
    } else {
        err->expected_tokens = NULL;
    }
    error_count++;
}

static void print_errors_json(void) {
    printf("{\n");
    printf("  \"errors\": [\n");
    for (int i = 0; i < error_count; i++) {
        ErrorInfo* err = &errors[i];
        printf("    {\n");
        printf("      \"line\": %d,\n", err->line);
        printf("      \"message\": \"%s\",\n", err->message);
        if (err->actual_token) {
            printf("      \"actual\": \"%s\",\n", err->actual_token);
        }
        printf("      \"expected\": [");
        for (int j = 0; j < err->expected_count; j++) {
            printf("\"%s\"", err->expected_tokens[j]);
            if (j < err->expected_count - 1) printf(", ");
        }
        printf("]\n");
        printf("    }%s\n", i < error_count - 1 ? "," : "");
    }
    printf("  ],\n");
    printf("  \"errorCount\": %d\n", error_count);
    printf("}\n");
}

/* 用户声明代码 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <errno.h>

/* ============ AST ============ */
typedef enum {
    AST_PROGRAM,
    AST_DECL_LIST,
    AST_VAR_DECL,
    AST_FUN_DECL,
    AST_TYPE_SPEC,
    AST_PARAMS,
    AST_PARAM_LIST,
    AST_PARAM,
    AST_COMPOUND_STMT,
    AST_LOCAL_DECLS,
    AST_LOCAL_DECL,
    AST_STMT_LIST,
    AST_STMT,
    AST_EXPR_STMT,
    AST_BLOCK_STMT,
    AST_IF_STMT,
    AST_WHILE_STMT,
    AST_RETURN_STMT,
    AST_CONTINUE_STMT,
    AST_BREAK_STMT,
    AST_EXPR,
    AST_BINARY_EXPR,
    AST_UNARY_EXPR,
    AST_ASSIGN_EXPR,
    AST_ARRAY_ACCESS,
    AST_FUNC_CALL,
    AST_IDENT,
    AST_INT_LITERAL,
    AST_ARG_LIST
} NodeType;

typedef struct ASTNode {
    NodeType type;
    char* value;
    char* op;
    struct ASTNode* child;
    struct ASTNode* sibling;
} ASTNode;

ASTNode* ast_root = NULL;
int node_count = 0;

/* ============ Symbol Table ============ */
typedef enum { SCOPE_GLOBAL, SCOPE_LOCAL, SCOPE_PARAM } Scope;

typedef struct Symbol {
    char* name;
    char* label;          /* only for global */
    Scope scope;
    char* func;           /* function name for local/param */
    int offset;           /* relative to s0, negative */
    int array_size;       /* for real arrays; 0 means scalar or pointer */
    bool is_param_array;  /* param array treated as pointer */
    struct Symbol* next;
} Symbol;

static Symbol* global_head = NULL;
static Symbol* global_tail = NULL;

static Symbol* local_head = NULL; /* includes params + locals for current function */
static Symbol* local_tail = NULL;

/* temp regs */
static const char* temp_registers[8] = {"t0","t1","t2","t3","t4","t5","t6","a0"};
static int temp_reg_top = 0;

static int label_counter = 0;
static const char* current_ret_label = NULL;

/* ============ AST helpers ============ */
static ASTNode* create_node(NodeType type, char* value) {
    ASTNode* node = (ASTNode*)malloc(sizeof(ASTNode));
    if (!node) { fprintf(stderr, "Error: malloc failed\n"); exit(1); }
    node->type = type;
    node->value = value ? strdup(value) : NULL;
    node->op = NULL;
    node->child = NULL;
    node->sibling = NULL;
    node_count++;
    return node;
}

static void add_child(ASTNode* parent, ASTNode* child) {
    if (!parent || !child) return;
    if (!parent->child) parent->child = child;
    else {
        ASTNode* s = parent->child;
        while (s->sibling) s = s->sibling;
        s->sibling = child;
    }
}

static void free_ast(ASTNode* node) {
    if (!node) return;
    free_ast(node->child);
    free_ast(node->sibling);
    if (node->value) free(node->value);
    if (node->op) free(node->op);
    free(node);
}

/* ============ Symbol helpers ============ */
static void clear_locals(void) {
    Symbol* s = local_head;
    while (s) {
        Symbol* n = s->next;
        free(s->name);
        if (s->func) free(s->func);
        free(s);
        s = n;
    }
    local_head = local_tail = NULL;
}

static void free_globals(void) {
    Symbol* s = global_head;
    while (s) {
        Symbol* n = s->next;
        free(s->name);
        free(s->label);
        free(s);
        s = n;
    }
    global_head = global_tail = NULL;
}

static Symbol* find_global(const char* name) {
    for (Symbol* s = global_head; s; s = s->next)
        if (strcmp(s->name, name) == 0) return s;
    return NULL;
}

static Symbol* add_global(const char* name) {
    Symbol* s = find_global(name);
    if (s) return s;

    s = (Symbol*)malloc(sizeof(Symbol));
    if (!s) { fprintf(stderr, "Error: malloc failed\n"); exit(1); }

    s->name = strdup(name);
    s->label = (char*)malloc(strlen(name) + 5);
    sprintf(s->label, "var_%s", name);
    s->scope = SCOPE_GLOBAL;
    s->func = NULL;
    s->offset = 0;
    s->array_size = 0;
    s->is_param_array = false;
    s->next = NULL;

    if (!global_head) global_head = global_tail = s;
    else global_tail = global_tail->next = s;
    return s;
}

static Symbol* find_local(const char* name) {
    for (Symbol* s = local_head; s; s = s->next)
        if (strcmp(s->name, name) == 0) return s;
    return NULL;
}

static Symbol* add_local(const char* func, const char* name, int offset,
                         int array_size, Scope scope, bool is_param_array) {
    Symbol* s = (Symbol*)malloc(sizeof(Symbol));
    if (!s) { fprintf(stderr, "Error: malloc failed\n"); exit(1); }

    s->name = strdup(name);
    s->label = NULL;
    s->scope = scope;
    s->func = func ? strdup(func) : NULL;
    s->offset = offset;
    s->array_size = array_size;
    s->is_param_array = is_param_array;
    s->next = NULL;

    if (!local_head) local_head = local_tail = s;
    else local_tail = local_tail->next = s;
    return s;
}

/* ============ temp regs ============ */
static const char* acquire_temp_register(void) {
    if (temp_reg_top >= 8) { fprintf(stderr, "Error: temp regs exhausted\n"); exit(1); }
    return temp_registers[temp_reg_top++];
}
static void release_temp_register(const char* reg) {
    (void)reg;
    if (temp_reg_top > 0) temp_reg_top--;
}

/* ============ labels ============ */
static char* generate_label(const char* prefix) {
    char* label = (char*)malloc(64);
    snprintf(label, 64, "%s%d", prefix, label_counter++);
    return label;
}

/* ============ collect globals ============ */
static void collect_globals(ASTNode* node) {
    if (!node) return;

    if (node->type == AST_VAR_DECL) {
        ASTNode* id = NULL;
        ASTNode* size = NULL;
        for (ASTNode* c = node->child; c; c = c->sibling) {
            if (c->type == AST_IDENT) id = c;
            else if (c->type == AST_INT_LITERAL) size = c;
        }
        if (id && id->value) {
            Symbol* g = add_global(id->value);
            if (node->value && strcmp(node->value, "array") == 0 && size && size->value)
                g->array_size = atoi(size->value);
        }
    }

    collect_globals(node->child);
    collect_globals(node->sibling);
}

/* ============ build params/locals on stack ============ */
/* layout: offsets are negative relative to s0 */
static int build_params(ASTNode* params, const char* func, int start_offset_bytes) {
    int offset = start_offset_bytes; /* already allocated bytes */

    if (!params) return offset;

    ASTNode* first = NULL;
    if (params->type == AST_PARAM_LIST) first = params->child;
    else if (params->type == AST_PARAMS) {
        if (params->value && strcmp(params->value, "void") == 0) return offset;
        first = params->child;
    }

    for (ASTNode* p = first; p; p = p->sibling) {
        if (p->type != AST_PARAM) continue;

        ASTNode* id = NULL;
        ASTNode* size = NULL;
        for (ASTNode* c = p->child; c; c = c->sibling) {
            if (c->type == AST_IDENT) id = c;
            else if (c->type == AST_INT_LITERAL) size = c;
        }
        if (!id || !id->value) continue;

        bool is_arr = (p->value && strcmp(p->value, "array") == 0);

        /* param scalar => 4 bytes slot
           param array => treat as pointer => 4 bytes slot */
        offset += 4;
        add_local(func, id->value, -offset, 0, SCOPE_PARAM, is_arr);
        (void)size;
    }

    return offset;
}

static int build_locals(ASTNode* local_decls, const char* func, int start_offset_bytes) {
    int offset = start_offset_bytes;

    if (!local_decls || local_decls->type != AST_LOCAL_DECLS) return offset;

    for (ASTNode* d = local_decls->child; d; d = d->sibling) {
        if (d->type != AST_LOCAL_DECL) continue;

        ASTNode* id = NULL;
        ASTNode* size = NULL;
        for (ASTNode* c = d->child; c; c = c->sibling) {
            if (c->type == AST_IDENT) id = c;
            else if (c->type == AST_INT_LITERAL) size = c;
        }
        if (!id || !id->value) continue;

        int bytes = 4;
        int arr = 0;
        if (d->value && strcmp(d->value, "array") == 0 && size && size->value) {
            arr = atoi(size->value);
            bytes = arr * 4;
        }

        offset += bytes;
        add_local(func, id->value, -offset, arr, SCOPE_LOCAL, false);
    }

    return offset;
}

static int align16(int x) { return (x + 15) & ~15; }

/* prologue/epilogue (s0 as fp) */
static void emit_prologue(FILE* out, int frame) {
    /* reserve frame + save ra/s0 (8 bytes) */
    fprintf(out, "    addi sp, sp, -%d\n", frame + 8);
    fprintf(out, "    sw ra, %d(sp)\n", frame + 4);
    fprintf(out, "    sw s0, %d(sp)\n", frame + 0);
    fprintf(out, "    addi s0, sp, %d\n", frame + 8);
}

static void emit_epilogue(FILE* out, int frame) {
    fprintf(out, "    lw s0, %d(sp)\n", frame + 0);
    fprintf(out, "    lw ra, %d(sp)\n", frame + 4);
    fprintf(out, "    addi sp, sp, %d\n", frame + 8);
    fprintf(out, "    jalr x0, 0(ra)\n");
}

/* store incoming a0-a7 into param slots */
static void spill_params(FILE* out) {
    int idx = 0;
    for (Symbol* s = local_head; s; s = s->next) {
        if (s->scope != SCOPE_PARAM) continue;
        if (idx >= 8) break;
        fprintf(out, "    sw a%d, %d(s0)\n", idx, s->offset);
        idx++;
    }
}

/* ============ codegen expressions ============ */
static const char* generate_expression(ASTNode* node, FILE* out);

static const char* load_identifier(const char* name, FILE* out) {
    const char* reg = acquire_temp_register();

    // 1) 先查局部（含形参）
    Symbol* l = find_local(name);
    if (l) {
        // 1.1 局部“真数组”：栈上分配了一段空间，IDENT 应该给出首地址
        if (l->array_size > 0 && !l->is_param_array) {
            // reg = &local_array[0]
            fprintf(out, "    addi %s, s0, %d\n", reg, l->offset);
            return reg;
        }

        // 1.2 参数数组（指针）或普通局部变量：栈槽里存的是“值”
        // 参数数组：值就是 base pointer；普通变量：值就是 int
        fprintf(out, "    lw %s, %d(s0)\n", reg, l->offset);
        return reg;
    }

    // 2) 再查全局
    Symbol* g = find_global(name);
    if (!g) g = add_global(name); // 容错

    // 2.1 全局数组：IDENT 给出地址
    if (g->array_size > 0) {
        fprintf(out, "    auipc %s, %%pcrel_hi(%s)\n", reg, g->label);
        fprintf(out, "    addi  %s, %s, %%pcrel_lo(%s)\n", reg, reg, g->label);
        return reg;
    }

    // 2.2 普通全局变量：先取地址再解引用
    fprintf(out, "    auipc %s, %%pcrel_hi(%s)\n", reg, g->label);
    fprintf(out, "    addi  %s, %s, %%pcrel_lo(%s)\n", reg, reg, g->label);
    fprintf(out, "    lw %s, 0(%s)\n", reg, reg);
    return reg;
}


// static const char* load_identifier(const char* name, FILE* out) {
//     const char* reg = acquire_temp_register();
//     Symbol* l = find_local(name);
//     if (l) {
//         fprintf(out, "    lw %s, %d(s0)\n", reg, l->offset);
//         return reg;
//     }
//     Symbol* g = find_global(name);
//     if (!g) g = add_global(name); /* tolerate */
//     fprintf(out, "    auipc %s, %%pcrel_hi(%s)\n", reg, g->label);
//     fprintf(out, "    addi  %s, %s, %%pcrel_lo(%s)\n", reg, reg, g->label);
//     fprintf(out, "    lw %s, 0(%s)\n", reg, reg);
//     return reg;
// }

static int32_t parse_imm32_rv(const char* lit) {
    // 允许十进制或 0x..（如果你未来会传 hex 字符串）
    errno = 0;
    char* end = NULL;

    // 用 unsigned 来解析更稳：能吃下 "4294966368" 这种数
    unsigned long long u = strtoull(lit, &end, 0);
    (void)end; // 若你想严格校验，可检查 *end == '\0'

    // RV32 语义：截断到 32 位，再按有符号解释
    uint32_t u32 = (uint32_t)u;
    return (int32_t)u32;
}

static int fits_i12_i32(int32_t v) {
    return (v >= -2048 && v <= 2047);
}

static const char* load_integer_literal(const char* lit, FILE* out) {
    const char* reg = acquire_temp_register();

    int32_t sv = parse_imm32_rv(lit);

    if (fits_i12_i32(sv)) {
        // 1条指令：addi
        fprintf(out, "    addi %s, x0, %d\n", reg, (int)sv);
    } else {
        // 2条指令：lui + addi（带 0x800 修正）
        // 这里用 int32_t 参与运算，确保是 RV32 的算术右移语义
        int32_t hi = (sv + 0x800) >> 12;
        int32_t lo = sv - (hi << 12);   // 必落在 [-2048, 2047]

        // hi 必在 [-524288, 524287]，符合 LUI 的 20-bit 有符号立即数
        fprintf(out, "    lui  %s, %d\n", reg, (int)hi);
        fprintf(out, "    addi %s, %s, %d\n", reg, reg, (int)lo);
    }

    return reg;
}

static void emit_array_base(ASTNode* array_id, FILE* out, const char* base_reg) {
    /* local array: base = s0 + offset
       param array(pointer): load pointer from slot into base_reg
       global: la base_reg, label */
    Symbol* l = find_local(array_id->value);
    if (l) {
        if (l->scope == SCOPE_PARAM && l->is_param_array) {
            fprintf(out, "    lw %s, %d(s0)\n", base_reg, l->offset);
        } else {
            fprintf(out, "    addi %s, s0, %d\n", base_reg, l->offset);
        }
        return;
    }
    Symbol* g = find_global(array_id->value);
    if (!g) g = add_global(array_id->value);
    fprintf(out, "    auipc %s, %%pcrel_hi(%s)\n", base_reg, g->label);
    fprintf(out, "    addi  %s, %s, %%pcrel_lo(%s)\n", base_reg, base_reg, g->label);
}

static const char* generate_binary_expr(ASTNode* node, FILE* out) {
    if (!node->child || !node->child->sibling) return NULL;

    const char* left = generate_expression(node->child, out);
    const char* right = generate_expression(node->child->sibling, out);
    const char* res = acquire_temp_register();

    const char* op = node->op ? node->op : "";

    if (strcmp(op, "+") == 0) fprintf(out, "    add %s, %s, %s\n", res, left, right);
    else if (strcmp(op, "-") == 0) fprintf(out, "    sub %s, %s, %s\n", res, left, right);
    else if (strcmp(op, "*") == 0) fprintf(out, "    mul %s, %s, %s\n", res, left, right);
    else if (strcmp(op, "/") == 0) fprintf(out, "    div %s, %s, %s\n", res, left, right);
    else if (strcmp(op, "%") == 0) fprintf(out, "    rem %s, %s, %s\n", res, left, right);
    else if (strcmp(op, "&") == 0) fprintf(out, "    and %s, %s, %s\n", res, left, right);
    else if (strcmp(op, "|") == 0) fprintf(out, "    or  %s, %s, %s\n", res, left, right);
    else if (strcmp(op, "^") == 0) fprintf(out, "    xor %s, %s, %s\n", res, left, right);
    else if (strcmp(op, "<<") == 0) fprintf(out, "    sll %s, %s, %s\n", res, left, right);
    else if (strcmp(op, ">>") == 0) fprintf(out, "    sra %s, %s, %s\n", res, left, right);
    else if (strcmp(op, "<") == 0) fprintf(out, "    slt %s, %s, %s\n", res, left, right);
    else if (strcmp(op, ">") == 0) fprintf(out, "    slt %s, %s, %s\n", res, right, left);
    else if (strcmp(op, "==") == 0) {
        fprintf(out, "    sub %s, %s, %s\n", res, left, right);
        fprintf(out, "    sltiu %s, %s, 1\n", res, res);
    } else if (strcmp(op, "!=") == 0) {
        fprintf(out, "    sub  %s, %s, %s\n", res, left, right);
        fprintf(out, "    sltu %s, x0, %s\n", res, res);
    } else if (strcmp(op, "<=") == 0) {
        fprintf(out, "    slt %s, %s, %s\n", res, right, left);
        fprintf(out, "    xori %s, %s, 1\n", res, res);
    } else if (strcmp(op, ">=") == 0) {
        fprintf(out, "    slt %s, %s, %s\n", res, left, right);
        fprintf(out, "    xori %s, %s, 1\n", res, res);
    } else if (strcmp(op, "&&") == 0) {
        fprintf(out, "    sltu %s, x0, %s\n", left, left);
        fprintf(out, "    sltu %s, x0, %s\n", right, right);
        fprintf(out, "    and  %s, %s, %s\n", res, left, right);
    } else if (strcmp(op, "||") == 0) {
        fprintf(out, "    or   %s, %s, %s\n", res, left, right);
        fprintf(out, "    sltu %s, x0, %s\n", res, res);
    }

    release_temp_register(right);
    release_temp_register(left);
    return res;
}

static const char* generate_unary_expr(ASTNode* node, FILE* out) {
    if (!node->child) return NULL;
    const char* op = node->op ? node->op : "";

    /* memory deref: $addr */
    if (strcmp(op, "$") == 0) {
        const char* addr = generate_expression(node->child, out);
        const char* res = acquire_temp_register();
        fprintf(out, "    lw %s, 0(%s)\n", res, addr);
        release_temp_register(addr);
        return res;
    }

    const char* x = generate_expression(node->child, out);
    const char* res = acquire_temp_register();

    if (strcmp(op, "-") == 0) fprintf(out, "    sub %s, x0, %s\n", res, x);
    else if (strcmp(op, "!") == 0) fprintf(out, "    sltiu %s, %s, 1\n", res, x);
    else if (strcmp(op, "~") == 0) fprintf(out, "    xori %s, %s, -1\n", res, x);
    else if (strcmp(op, "+") == 0) fprintf(out, "    addi  %s, %s, 0\n", res, x);

    release_temp_register(x);
    return res;
}

static const char* generate_expression(ASTNode* node, FILE* out) {
    if (!node) return NULL;

    switch (node->type) {
        case AST_IDENT:
            return load_identifier(node->value, out);

        case AST_INT_LITERAL:
            return load_integer_literal(node->value, out);

        case AST_BINARY_EXPR:
            return generate_binary_expr(node, out);

        case AST_UNARY_EXPR:
            return generate_unary_expr(node, out);

        case AST_ARRAY_ACCESS: {
            if (!node->child || !node->child->sibling) return NULL;
            ASTNode* array_id = node->child;
            ASTNode* index = node->child->sibling;

            const char* idx = generate_expression(index, out);
            const char* base = acquire_temp_register();
            emit_array_base(array_id, out, base);

            const char* off = acquire_temp_register();
            fprintf(out, "    slli %s, %s, 2\n", off, idx);

            const char* addr = acquire_temp_register();
            fprintf(out, "    add %s, %s, %s\n", addr, base, off);

            const char* res = acquire_temp_register();
            fprintf(out, "    lw %s, 0(%s)\n", res, addr);

            release_temp_register(addr);
            release_temp_register(off);
            release_temp_register(base);
            release_temp_register(idx);
            return res;
        }

        case AST_ASSIGN_EXPR: {
            if (!node->child || !node->child->sibling) return NULL;

            const char* v = generate_expression(node->child->sibling, out);
            if (!v) return NULL;

            /* memory assign: $addr = v */
            if (node->value && strcmp(node->value, "memory") == 0) {
                const char* addr = generate_expression(node->child, out);
                fprintf(out, "    sw %s, 0(%s)\n", v, addr);
                release_temp_register(addr);
                return v;
            }

            /* lhs */
            if (node->child->type == AST_IDENT) {
                const char* name = node->child->value;
                Symbol* l = find_local(name);
                if (l) {
                    fprintf(out, "    sw %s, %d(s0)\n", v, l->offset);
                } else {
                    Symbol* g = find_global(name);
                    if (!g) g = add_global(name);
                    fprintf(out, "    auipc t6, %%pcrel_hi(%s)\n", g->label);
                    fprintf(out, "    addi  t6, t6, %%pcrel_lo(%s)\n", g->label);
                    fprintf(out, "    sw %s, 0(t6)\n", v);
                }
                return v;
            }

            if (node->child->type == AST_ARRAY_ACCESS) {
                ASTNode* aa = node->child;
                if (!aa->child || !aa->child->sibling) return NULL;

                ASTNode* array_id = aa->child;
                ASTNode* index = aa->child->sibling;

                const char* idx = generate_expression(index, out);
                const char* base = acquire_temp_register();
                emit_array_base(array_id, out, base);

                const char* off = acquire_temp_register();
                fprintf(out, "    slli %s, %s, 2\n", off, idx);

                const char* addr = acquire_temp_register();
                fprintf(out, "    add %s, %s, %s\n", addr, base, off);

                fprintf(out, "    sw %s, 0(%s)\n", v, addr);

                release_temp_register(addr);
                release_temp_register(off);
                release_temp_register(base);
                release_temp_register(idx);
                return v;
            }

            return v;
        }

        case AST_FUNC_CALL: {
            /* simplified: pass up to 8 args in a0-a7 */
            if (node->child && node->child->type == AST_ARG_LIST) {
                ASTNode* arg = node->child->child;
                int k = 0;
                while (arg && k < 8) {
                    const char* r = generate_expression(arg, out);
                    if (r) {
                        if (strcmp(r, "a0") != 0) {
                            fprintf(out, "    addi a%d, %s, 0\n", k, r);
                        }
                        release_temp_register(r);
                    }
                    k++;
                    arg = arg->sibling;
                }
            }

            if (node->value) fprintf(out, "    jal ra, %s\n", node->value);

            const char* res = acquire_temp_register();
            fprintf(out, "    addi %s, a0, 0\n", res);
            return res;
        }

        default:
            return NULL;
    }
}

/* ============ codegen statements ============ */
static void generate_statement(ASTNode* node, FILE* out, bool* has_return);

static void generate_statement_list(ASTNode* node, FILE* out, bool* has_return) {
    if (!node) return;
    for (ASTNode* s = node->child; s; s = s->sibling)
        generate_statement(s, out, has_return);
}

static void generate_return_statement(ASTNode* node, FILE* out, bool* has_return) {
    *has_return = true;

    if (node->child) {
        const char* r = generate_expression(node->child, out);
        fprintf(out, "    addi a0, %s, 0\n", r);
        release_temp_register(r);
    } else {
        fprintf(out, "    addi a0, x0, 0\n");
    }
    fprintf(out, "    jal x0, %s\n", current_ret_label);
}

static void generate_if_statement(ASTNode* node, FILE* out, bool* has_return) {
    if (!node->child) return;

    char* else_label = generate_label(".L_else");
    char* end_label  = generate_label(".L_end_if");

    const char* cond = generate_expression(node->child, out);
    bool has_else = (node->child->sibling && node->child->sibling->sibling);

    fprintf(out, "    beq %s, x0, %s\n", cond, has_else ? else_label : end_label);
    release_temp_register(cond);

    if (node->child->sibling)
        generate_statement(node->child->sibling, out, has_return);

    if (has_else) {
        fprintf(out, "    jal x0, %s\n", end_label);
        fprintf(out, "%s:\n", else_label);
        generate_statement(node->child->sibling->sibling, out, has_return);
    }

    fprintf(out, "%s:\n", end_label);
    free(else_label);
    free(end_label);
}

static void generate_while_statement(ASTNode* node, FILE* out, bool* has_return) {
    if (!node->child) return;

    char* loop_label = generate_label(".lop");
    char* end_label  = generate_label(".end");

    fprintf(out, "%s:\n", loop_label);

    const char* cond = generate_expression(node->child, out);
    fprintf(out, "    beq %s, x0, %s\n", cond, end_label);
    release_temp_register(cond);

    if (node->child->sibling)
        generate_statement(node->child->sibling, out, has_return);

    fprintf(out, "    jal x0, %s\n", loop_label);
    fprintf(out, "%s:\n", end_label);

    free(loop_label);
    free(end_label);
}

static void generate_statement(ASTNode* node, FILE* out, bool* has_return) {
    if (!node) return;

    switch (node->type) {
        case AST_EXPR_STMT: {
            if (node->child) {
                const char* r = generate_expression(node->child, out);
                if (r) release_temp_register(r);
            }
            break;
        }
        case AST_RETURN_STMT:
            generate_return_statement(node, out, has_return);
            break;
        case AST_IF_STMT:
            generate_if_statement(node, out, has_return);
            break;
        case AST_WHILE_STMT:
            generate_while_statement(node, out, has_return);
            break;
        case AST_BLOCK_STMT:
        case AST_STMT_LIST:
            generate_statement_list(node, out, has_return);
            break;
        case AST_CONTINUE_STMT:
        case AST_BREAK_STMT:
            /* TODO */
            break;
        default:
            break;
    }
}

/* ============ emit sections ============ */
static void emit_data_section(FILE* out) {
    fprintf(out, ".data\n");
    for (Symbol* s = global_head; s; s = s->next) {
        if (s->array_size > 0) {
            fprintf(out, "%s: .word", s->label);
            for (int i = 0; i < s->array_size; i++) {
                fprintf(out, " 0%s", (i == s->array_size - 1) ? "" : ",");
            }
            fprintf(out, "\n");
        } else {
            fprintf(out, "%s: .word 0\n", s->label);
        }
    }
    fprintf(out, "\n");
}

static void emit_text_section(ASTNode* root, FILE* out) {
    fprintf(out, ".text\n");
    if (!root || !root->child || root->child->type != AST_DECL_LIST) return;

    for (ASTNode* decl = root->child->child; decl; decl = decl->sibling) {
        if (decl->type != AST_FUN_DECL) continue;

        ASTNode* type_node  = decl->child;
        ASTNode* ident_node = type_node ? type_node->sibling : NULL;
        ASTNode* params     = ident_node ? ident_node->sibling : NULL;
        ASTNode* compound   = params ? params->sibling : NULL;

        if (!compound || compound->type != AST_COMPOUND_STMT) continue;
        if (!ident_node || ident_node->type != AST_IDENT || !ident_node->value) continue;

        const char* fname = ident_node->value;

        if (strcmp(fname, "main") == 0) fprintf(out, ".globl main\n");
        fprintf(out, "%s:\n", fname);
        if (strcmp(fname, "main") == 0) fprintf(out, "    lui sp, 0x00010\n");
        /* compound children: local_decls, stmt_list */
        ASTNode* local_decls = compound->child; /* AST_LOCAL_DECLS */
        ASTNode* stmt_list   = local_decls ? local_decls->sibling : NULL; /* AST_STMT_LIST */

        clear_locals();

        int used = 8;
        used = build_params(params, fname, used);
        used = build_locals(local_decls, fname, used);
        int frame = align16(used);

        char* ret_label = (char*)malloc(96);
        snprintf(ret_label, 96, ".L_ret_%s_%d", fname, label_counter++);
        current_ret_label = ret_label;

        emit_prologue(out, frame);
        spill_params(out);

        bool has_return = false;
        if (stmt_list && stmt_list->type == AST_STMT_LIST) {
            generate_statement_list(stmt_list, out, &has_return);
        }

        if (!has_return) {
            fprintf(out, "    addi a0, x0, 0\n");
            fprintf(out, "    jal x0, %s\n", current_ret_label);
        }

        fprintf(out, "%s:\n", current_ret_label);
        emit_epilogue(out, frame);

        fprintf(out, "\n");
        free(ret_label);
        current_ret_label = NULL;
        clear_locals();
    }
}

/* ============ entry ============ */
static void generate_assembly(ASTNode* root) {
    if (!root) return;

    FILE* out = fopen("output.asm", "w");
    if (!out) {
        fprintf(stderr, "Error: cannot open output.asm\n");
        return;
    }

    collect_globals(root);
    emit_data_section(out);
    emit_text_section(root, out);

    fclose(out);

    free_globals();
}

/* bison/flex */
extern int yylex();
extern int yylineno;
extern char yytext[];

void yyerror(const char* s) {
    fprintf(stderr, "Error at line %d: %s\n", yylineno, s);
}



/* 全局变量定义 */
YYSTYPE yylval;

/* 解析器内部定义 */
#ifndef YYMAXDEPTH
# define YYMAXDEPTH 10000
#endif

#define YYFINAL 442
#define YYLAST 19492

#define YYNTOKENS 44
#define YYNNTS 25
#define YYNRULES 78
#define YYNSTATES 443
#define YYMAXUTOK 276
#define YYUNDEF -1

static const short yytranslate_table[277] = {
  0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  -1, 42, -1, -1, 30, 10, 18, -1, 1, 9, 12, 16, 4, 15, -1, 41, 
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 11, 19, 5, 17, -1, 
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 3, -1, 6, 13, -1, 
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 2, 14, 39, 31, -1, 
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  7, 8, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 32, 33, 34, 35, 
  36, 37, 38, 40, 43
};

static inline int yytranslate_token(int token) {
  if (token < 0 || token > YYMAXUTOK) {
    return YYUNDEF;
  }
  return yytranslate_table[token];
}

/* 解析表 */
static const short yytable[] = {
  /* 状态 0 */
  -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 1, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 2, -32767, -32767, -32767, -32767, -32767, 
  /* 状态 1 */
  -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -10, -32767, -32767, -32767, 
  /* 状态 2 */
  -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -9, -32767, -32767, -32767, 
  /* 状态 3 */
  -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 10, -32767, -32767, -32767, 
  /* 状态 4 */
  0, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 
  /* 状态 5 */
  -6, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -6, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -6, -32767, -32767, -32767, -32767, -32767, 
  /* 状态 6 */
  -5, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -5, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -5, -32767, -32767, -32767, -32767, -32767, 
  /* 状态 7 */
  -4, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -4, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -4, -32767, -32767, -32767, -32767, -32767, 
  /* 状态 8 */
  -2, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 1, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 2, -32767, -32767, -32767, -32767, -32767, 
  /* 状态 9 */
  -3, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -3, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -3, -32767, -32767, -32767, -32767, -32767, 
  /* 状态 10 */
  -32767, 12, -32767, 11, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 13, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 
  /* 状态 11 */
  -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 429, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 430, 
  /* 状态 12 */
  -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 1, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 17, -32767, -32767, -32767, -32767, -32767, 
  /* 状态 13 */
  -7, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -7, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -7, -32767, -32767, -32767, -32767, -32767, 
  /* 状态 14 */
  -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 436, -32767, -32767, -32767, 
  /* 状态 15 */
  -32767, -32767, -32767, -32767, 434, -32767, -32767, -32767, -32767, -13, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 
  /* 状态 16 */
  -32767, -32767, -32767, -32767, -16, -32767, -32767, -32767, -32767, -16, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 
  /* 状态 17 */
  -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -14, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -9, -32767, -32767, -32767, 
  /* 状态 18 */
  -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 19, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 
  /* 状态 19 */
  -32767, -32767, 21, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 20, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 
  /* 状态 20 */
  -12, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -12, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -12, -32767, -32767, -32767, -32767, -32767, 
  /* 状态 21 */
  -32767, -32767, -36, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -36, -32767, -32767, -32767, -32767, -32767, -36, -32767, -36, -36, -32767, -32767, -36, -36, -32767, -36, -32767, -36, -36, -36, -32767, -32767, -32767, 
  /* 状态 22 */
  -11, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -11, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -11, -32767, -32767, -32767, -32767, -32767, 
  /* 状态 23 */
  -32767, -32767, -20, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 1, -32767, -32767, -32767, -32767, -32767, -20, -32767, -20, -20, -32767, -32767, -20, -20, -32767, -20, -32767, 2, -20, -20, -32767, -32767, -32767, 
  /* 状态 24 */
  -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 426, -32767, -32767, -32767, 
  /* 状态 25 */
  -32767, -32767, -35, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -35, -32767, -32767, -32767, -32767, -32767, -35, -32767, -35, -35, -32767, -32767, -35, -35, -32767, -35, -32767, -35, -35, -35, -32767, -32767, -32767, 
  /* 状态 26 */
  -32767, -32767, 40, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 27, -32767, 39, 32, -32767, -32767, 28, 29, -32767, 30, -32767, -32767, 43, 35, -32767, -32767, -32767, 
  /* 状态 27 */
  -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 425, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 
  /* 状态 28 */
  -32767, 58, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 422, -32767, -32767, -32767, 57, 56, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 55, 52, 51, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 54, -32767, 59, 50, 
  /* 状态 29 */
  -32767, 418, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 
  /* 状态 30 */
  -32767, 363, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 
  /* 状态 31 */
  -32767, -32767, -19, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -19, -32767, -19, -19, -32767, -32767, -19, -19, -32767, -19, -32767, -32767, -19, -19, -32767, -32767, -32767, 
  /* 状态 32 */
  -32767, 308, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 307, 306, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 305, 302, 301, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 304, -32767, 309, 300, 
  /* 状态 33 */
  -32767, -32767, -23, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -23, -32767, -23, -23, -32767, -32767, -23, -23, -32767, -23, -32767, -32767, -23, -23, -32767, -32767, -32767, 
  /* 状态 34 */
  -32767, -32767, -25, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -25, -32767, -25, -25, -32767, -32767, -25, -25, -32767, -25, -32767, -32767, -25, -25, -32767, -32767, -32767, 
  /* 状态 35 */
  -32767, 47, -32767, 48, -32767, 49, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 
  /* 状态 36 */
  -32767, -32767, -27, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -27, -32767, -27, -27, -32767, -32767, -27, -27, -32767, -27, -32767, -32767, -27, -27, -32767, -32767, -32767, 
  /* 状态 37 */
  -32767, -32767, -26, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -26, -32767, -26, -26, -32767, -32767, -26, -26, -32767, -26, -32767, -32767, -26, -26, -32767, -32767, -32767, 
  /* 状态 38 */
  -32767, -32767, -22, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -22, -32767, -22, -22, -32767, -32767, -22, -22, -32767, -22, -32767, -32767, -22, -22, -32767, -32767, -32767, 
  /* 状态 39 */
  -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 46, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 
  /* 状态 40 */
  -32767, -32767, -20, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -20, -32767, -20, -20, -32767, -32767, -20, -20, -32767, -20, -32767, -32767, -20, -20, -32767, -32767, -32767, 
  /* 状态 41 */
  -32767, -32767, -21, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -21, -32767, -21, -21, -32767, -32767, -21, -21, -32767, -21, -32767, -32767, -21, -21, -32767, -32767, -32767, 
  /* 状态 42 */
  -32767, -32767, -24, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -24, -32767, -24, -24, -32767, -32767, -24, -24, -32767, -24, -32767, -32767, -24, -24, -32767, -32767, -32767, 
  /* 状态 43 */
  -34, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -34, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -34, -32767, -32767, -32767, -32767, -32767, 
  /* 状态 44 */
  -32767, -32767, 40, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 27, -32767, 39, 32, -32767, -32767, 28, 29, -32767, 30, -32767, -32767, 45, 35, -32767, -32767, -32767, 
  /* 状态 45 */
  -32767, -32767, -33, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -33, -32767, -33, -33, -32767, -32767, -33, -33, -32767, -33, -32767, -32767, -33, -33, -32767, -32767, -32767, 
  /* 状态 46 */
  -32767, -32767, -78, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -78, -32767, -78, -78, -32767, -32767, -78, -78, -32767, -78, -32767, -32767, -78, -78, -32767, -32767, -32767, 
  /* 状态 47 */
  -32767, 222, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -76, -32767, -32767, -32767, -32767, -32767, 220, 219, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 218, 215, 214, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 216, -32767, 223, 213, 
  /* 状态 48 */
  -32767, 163, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 162, 161, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 160, 157, 156, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 159, -32767, 164, 155, 
  /* 状态 49 */
  -32767, 58, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 57, 56, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 55, 52, 51, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 54, -32767, 59, 50, 
  /* 状态 50 */
  -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -72, -72, -72, -72, -72, -72, -72, -72, -72, -72, -32767, -32767, -72, -72, -72, -72, -72, -32767, -72, -32767, -32767, -32767, -32767, -32767, -32767, -72, -32767, -72, -32767, -32767, -32767, -72, -32767, -32767, 
  /* 状态 51 */
  -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -71, -71, -71, -71, -71, -71, -71, -71, -71, -71, -32767, -32767, -71, -71, -71, -71, -71, -32767, -71, -32767, -32767, -32767, -32767, -32767, -32767, -71, -32767, -71, -32767, -32767, -32767, -71, -32767, -32767, 
  /* 状态 52 */
  -32767, 58, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 57, 56, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 55, 52, 51, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 54, -32767, 59, 50, 
  /* 状态 53 */
  -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -64, -64, -64, -64, -64, -64, -64, -64, -64, -64, -32767, -32767, -64, -64, -64, -64, -64, -32767, -64, -32767, -32767, -32767, -32767, -32767, -32767, -64, -32767, -64, -32767, -32767, -32767, -64, -32767, -32767, 
  /* 状态 54 */
  -32767, 285, -32767, 286, -32767, -32767, -32767, -32767, -32767, -32767, -61, -61, -61, -61, -61, -61, -61, -61, -61, -61, -32767, -32767, -61, -61, -61, -61, -61, -32767, -61, -32767, -32767, -32767, -32767, -32767, -32767, -61, -32767, -61, -32767, -32767, -32767, -61, -32767, -32767, 
  /* 状态 55 */
  -32767, 58, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 57, 56, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 55, 52, 51, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 54, -32767, 59, 50, 
  /* 状态 56 */
  -32767, 58, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 57, 56, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 55, 52, 51, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 54, -32767, 59, 50, 
  /* 状态 57 */
  -32767, 58, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 57, 56, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 55, 52, 51, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 54, -32767, 59, 50, 
  /* 状态 58 */
  -32767, 107, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 106, 105, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 104, 101, 100, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 103, -32767, 108, 99, 
  /* 状态 59 */
  -32767, 58, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 57, 56, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 55, 52, 51, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 54, -32767, 59, 50, 
  /* 状态 60 */
  -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 63, 72, 66, 78, 61, 77, 67, 69, 73, 71, -32767, -32767, 68, 70, 62, 64, 74, -32767, 76, -32767, -32767, -32767, -32767, -32767, -32767, 75, -32767, 79, -32767, -32767, -32767, 65, -32767, -32767, 
  /* 状态 61 */
  -32767, 58, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 57, 56, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 55, 52, 51, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 54, -32767, 59, 50, 
  /* 状态 62 */
  -32767, 58, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 57, 56, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 55, 52, 51, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 54, -32767, 59, 50, 
  /* 状态 63 */
  -32767, 58, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 57, 56, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 55, 52, 51, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 54, -32767, 59, 50, 
  /* 状态 64 */
  -32767, 58, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 57, 56, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 55, 52, 51, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 54, -32767, 59, 50, 
  /* 状态 65 */
  -32767, 58, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 57, 56, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 55, 52, 51, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 54, -32767, 59, 50, 
  /* 状态 66 */
  -32767, 58, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 57, 56, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 55, 52, 51, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 54, -32767, 59, 50, 
  /* 状态 67 */
  -32767, 58, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 57, 56, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 55, 52, 51, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 54, -32767, 59, 50, 
  /* 状态 68 */
  -32767, 58, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 57, 56, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 55, 52, 51, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 54, -32767, 59, 50, 
  /* 状态 69 */
  -32767, 58, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 57, 56, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 55, 52, 51, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 54, -32767, 59, 50, 
  /* 状态 70 */
  -32767, 58, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 57, 56, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 55, 52, 51, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 54, -32767, 59, 50, 
  /* 状态 71 */
  -32767, 58, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 57, 56, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 55, 52, 51, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 54, -32767, 59, 50, 
  /* 状态 72 */
  -32767, -32767, -28, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -28, -32767, -28, -28, -32767, -32767, -28, -28, -32767, -28, -32767, -32767, -28, -28, -32767, -32767, -32767, 
  /* 状态 73 */
  -32767, 58, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 57, 56, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 55, 52, 51, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 54, -32767, 59, 50, 
  /* 状态 74 */
  -32767, 58, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 57, 56, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 55, 52, 51, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 54, -32767, 59, 50, 
  /* 状态 75 */
  -32767, 58, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 57, 56, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 55, 52, 51, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 54, -32767, 59, 50, 
  /* 状态 76 */
  -32767, 58, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 57, 56, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 55, 52, 51, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 54, -32767, 59, 50, 
  /* 状态 77 */
  -32767, 58, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 57, 56, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 55, 52, 51, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 54, -32767, 59, 50, 
  /* 状态 78 */
  -32767, 58, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 57, 56, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 55, 52, 51, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 54, -32767, 59, 50, 
  /* 状态 79 */
  -32767, 58, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 57, 56, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 55, 52, 51, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 54, -32767, 59, 50, 
  /* 状态 80 */
  -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 63, -43, 66, 78, 61, 77, 67, 69, 73, 71, -32767, -32767, 68, 70, 62, 64, 74, -32767, 76, -32767, -32767, -32767, -32767, -32767, -32767, 75, -32767, -43, -32767, -32767, -32767, 65, -32767, -32767, 
  /* 状态 81 */
  -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 63, -66, 66, -66, -66, -66, -66, -66, -66, -66, -32767, -32767, -66, -66, 62, 64, -66, -32767, -66, -32767, -32767, -32767, -32767, -32767, -32767, -66, -32767, -66, -32767, -32767, -32767, 65, -32767, -32767, 
  /* 状态 82 */
  -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 63, -52, 66, 78, 61, -52, -52, -52, 73, -52, -32767, -32767, -52, -52, 62, 64, -52, -32767, -52, -32767, -32767, -32767, -32767, -32767, -32767, -52, -32767, -52, -32767, -32767, -32767, 65, -32767, -32767, 
  /* 状态 83 */
  -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 63, -44, 66, 78, 61, 77, 67, -44, 73, -44, -32767, -32767, -44, -44, 62, 64, -44, -32767, -44, -32767, -32767, -32767, -32767, -32767, -32767, -44, -32767, -44, -32767, -32767, -32767, 65, -32767, -32767, 
  /* 状态 84 */
  -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 63, -46, 66, 78, 61, 77, 67, -46, 73, -46, -32767, -32767, -46, -46, 62, 64, -46, -32767, -46, -32767, -32767, -32767, -32767, -32767, -32767, -46, -32767, -46, -32767, -32767, -32767, 65, -32767, -32767, 
  /* 状态 85 */
  -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 63, -45, 66, 78, 61, 77, 67, -45, 73, -45, -32767, -32767, -45, -45, 62, 64, -45, -32767, -45, -32767, -32767, -32767, -32767, -32767, -32767, -45, -32767, -45, -32767, -32767, -32767, 65, -32767, -32767, 
  /* 状态 86 */
  -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 63, -65, 66, -65, -65, -65, -65, -65, -65, -65, -32767, -32767, -65, -65, 62, 64, -65, -32767, -65, -32767, -32767, -32767, -32767, -32767, -32767, -65, -32767, -65, -32767, -32767, -32767, 65, -32767, -32767, 
  /* 状态 87 */
  -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 63, -47, 66, 78, 61, 77, 67, -47, 73, -47, -32767, -32767, -47, -47, 62, 64, -47, -32767, -47, -32767, -32767, -32767, -32767, -32767, -32767, -47, -32767, -47, -32767, -32767, -32767, 65, -32767, -32767, 
  /* 状态 88 */
  -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 63, -48, 66, 78, 61, 77, 67, -48, 73, -48, -32767, -32767, -48, -48, 62, 64, -48, -32767, -48, -32767, -32767, -32767, -32767, -32767, -32767, -48, -32767, -48, -32767, -32767, -32767, 65, -32767, -32767, 
  /* 状态 89 */
  -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 63, -49, 66, 78, 61, 77, 67, -49, 73, -49, -32767, -32767, -49, -49, 62, 64, -49, -32767, -49, -32767, -32767, -32767, -32767, -32767, -32767, -49, -32767, -49, -32767, -32767, -32767, 65, -32767, -32767, 
  /* 状态 90 */
  -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 63, -50, 66, 78, 61, 77, 67, 69, 73, 71, -32767, -32767, -50, 70, 62, 64, 74, -32767, 76, -32767, -32767, -32767, -32767, -32767, -32767, 75, -32767, -50, -32767, -32767, -32767, 65, -32767, -32767, 
  /* 状态 91 */
  -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 63, -51, 66, 78, 61, -51, -51, -51, 73, -51, -32767, -32767, -51, -51, 62, 64, -51, -32767, -51, -32767, -32767, -32767, -32767, -32767, -32767, -51, -32767, -51, -32767, -32767, -32767, 65, -32767, -32767, 
  /* 状态 92 */
  -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -53, -53, -53, -53, -53, -53, -53, -53, -53, -53, -32767, -32767, -53, -53, 62, 64, -53, -32767, -53, -32767, -32767, -32767, -32767, -32767, -32767, -53, -32767, -53, -32767, -32767, -32767, -53, -32767, -32767, 
  /* 状态 93 */
  -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -54, -54, -54, -54, -54, -54, -54, -54, -54, -54, -32767, -32767, -54, -54, 62, 64, -54, -32767, -54, -32767, -32767, -32767, -32767, -32767, -32767, -54, -32767, -54, -32767, -32767, -32767, -54, -32767, -32767, 
  /* 状态 94 */
  -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -69, -69, -69, -69, -69, -69, -69, -69, -69, -69, -32767, -32767, -69, -69, 62, 64, -69, -32767, -69, -32767, -32767, -32767, -32767, -32767, -32767, -69, -32767, -69, -32767, -32767, -32767, -69, -32767, -32767, 
  /* 状态 95 */
  -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -55, -55, -55, -55, -55, -55, -55, -55, -55, -55, -32767, -32767, -55, -55, 62, 64, -55, -32767, -55, -32767, -32767, -32767, -32767, -32767, -32767, -55, -32767, -55, -32767, -32767, -32767, -55, -32767, -32767, 
  /* 状态 96 */
  -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -68, -68, -68, -68, -68, -68, -68, -68, -68, -68, -32767, -32767, -68, -68, 62, 64, -68, -32767, -68, -32767, -32767, -32767, -32767, -32767, -32767, -68, -32767, -68, -32767, -32767, -32767, -68, -32767, -32767, 
  /* 状态 97 */
  -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 63, -70, 66, 78, -70, -70, -70, -70, 73, -70, -32767, -32767, -70, -70, 62, 64, -70, -32767, -70, -32767, -32767, -32767, -32767, -32767, -32767, -70, -32767, -70, -32767, -32767, -32767, 65, -32767, -32767, 
  /* 状态 98 */
  -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -56, -56, -56, -56, -56, -56, -56, -56, -56, -56, -32767, -32767, -56, -56, -56, -56, -56, -32767, -56, -32767, -32767, -32767, -32767, -32767, -32767, -56, -32767, -56, -32767, -32767, -32767, -56, -32767, -32767, 
  /* 状态 99 */
  -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -72, -72, -32767, -72, -72, -72, -72, -72, -72, -72, -72, -32767, -32767, -72, -72, -72, -72, -72, -32767, -72, -32767, -32767, -32767, -32767, -32767, -32767, -72, -32767, -72, -32767, -32767, -32767, -72, -32767, -32767, 
  /* 状态 100 */
  -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -71, -71, -32767, -71, -71, -71, -71, -71, -71, -71, -71, -32767, -32767, -71, -71, -71, -71, -71, -32767, -71, -32767, -32767, -32767, -32767, -32767, -32767, -71, -32767, -71, -32767, -32767, -32767, -71, -32767, -32767, 
  /* 状态 101 */
  -32767, 107, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 106, 105, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 104, 101, 100, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 103, -32767, 108, 99, 
  /* 状态 102 */
  -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -64, -64, -32767, -64, -64, -64, -64, -64, -64, -64, -64, -32767, -32767, -64, -64, -64, -64, -64, -32767, -64, -32767, -32767, -32767, -32767, -32767, -32767, -64, -32767, -64, -32767, -32767, -32767, -64, -32767, -32767, 
  /* 状态 103 */
  -32767, 153, -32767, 154, -32767, -32767, -32767, -32767, -32767, -61, -61, -32767, -61, -61, -61, -61, -61, -61, -61, -61, -32767, -32767, -61, -61, -61, -61, -61, -32767, -61, -32767, -32767, -32767, -32767, -32767, -32767, -61, -32767, -61, -32767, -32767, -32767, -61, -32767, -32767, 
  /* 状态 104 */
  -32767, 107, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 106, 105, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 104, 101, 100, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 103, -32767, 108, 99, 
  /* 状态 105 */
  -32767, 107, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 106, 105, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 104, 101, 100, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 103, -32767, 108, 99, 
  /* 状态 106 */
  -32767, 107, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 106, 105, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 104, 101, 100, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 103, -32767, 108, 99, 
  /* 状态 107 */
  -32767, 107, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 106, 105, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 104, 101, 100, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 103, -32767, 108, 99, 
  /* 状态 108 */
  -32767, 107, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 106, 105, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 104, 101, 100, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 103, -32767, 108, 99, 
  /* 状态 109 */
  -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 123, 112, -32767, 115, 127, 110, 126, 116, 118, 121, 120, -32767, -32767, 117, 119, 111, 113, 122, -32767, 125, -32767, -32767, -32767, -32767, -32767, -32767, 124, -32767, 128, -32767, -32767, -32767, 114, -32767, -32767, 
  /* 状态 110 */
  -32767, 107, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 106, 105, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 104, 101, 100, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 103, -32767, 108, 99, 
  /* 状态 111 */
  -32767, 107, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 106, 105, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 104, 101, 100, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 103, -32767, 108, 99, 
  /* 状态 112 */
  -32767, 107, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 106, 105, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 104, 101, 100, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 103, -32767, 108, 99, 
  /* 状态 113 */
  -32767, 107, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 106, 105, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 104, 101, 100, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 103, -32767, 108, 99, 
  /* 状态 114 */
  -32767, 107, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 106, 105, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 104, 101, 100, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 103, -32767, 108, 99, 
  /* 状态 115 */
  -32767, 107, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 106, 105, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 104, 101, 100, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 103, -32767, 108, 99, 
  /* 状态 116 */
  -32767, 107, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 106, 105, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 104, 101, 100, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 103, -32767, 108, 99, 
  /* 状态 117 */
  -32767, 107, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 106, 105, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 104, 101, 100, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 103, -32767, 108, 99, 
  /* 状态 118 */
  -32767, 107, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 106, 105, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 104, 101, 100, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 103, -32767, 108, 99, 
  /* 状态 119 */
  -32767, 107, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 106, 105, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 104, 101, 100, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 103, -32767, 108, 99, 
  /* 状态 120 */
  -32767, 107, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 106, 105, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 104, 101, 100, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 103, -32767, 108, 99, 
  /* 状态 121 */
  -32767, 107, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 106, 105, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 104, 101, 100, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 103, -32767, 108, 99, 
  /* 状态 122 */
  -32767, 107, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 106, 105, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 104, 101, 100, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 103, -32767, 108, 99, 
  /* 状态 123 */
  -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -60, -60, -60, -60, -60, -60, -60, -60, -60, -60, -32767, -32767, -60, -60, -60, -60, -60, -32767, -60, -32767, -32767, -32767, -32767, -32767, -32767, -60, -32767, -60, -32767, -32767, -32767, -60, -32767, -32767, 
  /* 状态 124 */
  -32767, 107, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 106, 105, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 104, 101, 100, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 103, -32767, 108, 99, 
  /* 状态 125 */
  -32767, 107, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 106, 105, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 104, 101, 100, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 103, -32767, 108, 99, 
  /* 状态 126 */
  -32767, 107, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 106, 105, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 104, 101, 100, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 103, -32767, 108, 99, 
  /* 状态 127 */
  -32767, 107, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 106, 105, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 104, 101, 100, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 103, -32767, 108, 99, 
  /* 状态 128 */
  -32767, 107, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 106, 105, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 104, 101, 100, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 103, -32767, 108, 99, 
  /* 状态 129 */
  -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -43, 112, -32767, 115, 127, 110, 126, 116, 118, 121, 120, -32767, -32767, 117, 119, 111, 113, 122, -32767, 125, -32767, -32767, -32767, -32767, -32767, -32767, 124, -32767, -43, -32767, -32767, -32767, 114, -32767, -32767, 
  /* 状态 130 */
  -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -66, 112, -32767, 115, -66, -66, -66, -66, -66, -66, -66, -32767, -32767, -66, -66, 111, 113, -66, -32767, -66, -32767, -32767, -32767, -32767, -32767, -32767, -66, -32767, -66, -32767, -32767, -32767, 114, -32767, -32767, 
  /* 状态 131 */
  -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -52, 112, -32767, 115, 127, 110, -52, -52, -52, 121, -52, -32767, -32767, -52, -52, 111, 113, -52, -32767, -52, -32767, -32767, -32767, -32767, -32767, -32767, -52, -32767, -52, -32767, -32767, -32767, 114, -32767, -32767, 
  /* 状态 132 */
  -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -44, 112, -32767, 115, 127, 110, 126, 116, -44, 121, -44, -32767, -32767, -44, -44, 111, 113, -44, -32767, -44, -32767, -32767, -32767, -32767, -32767, -32767, -44, -32767, -44, -32767, -32767, -32767, 114, -32767, -32767, 
  /* 状态 133 */
  -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -46, 112, -32767, 115, 127, 110, 126, 116, -46, 121, -46, -32767, -32767, -46, -46, 111, 113, -46, -32767, -46, -32767, -32767, -32767, -32767, -32767, -32767, -46, -32767, -46, -32767, -32767, -32767, 114, -32767, -32767, 
  /* 状态 134 */
  -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -45, 112, -32767, 115, 127, 110, 126, 116, -45, 121, -45, -32767, -32767, -45, -45, 111, 113, -45, -32767, -45, -32767, -32767, -32767, -32767, -32767, -32767, -45, -32767, -45, -32767, -32767, -32767, 114, -32767, -32767, 
  /* 状态 135 */
  -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -65, 112, -32767, 115, -65, -65, -65, -65, -65, -65, -65, -32767, -32767, -65, -65, 111, 113, -65, -32767, -65, -32767, -32767, -32767, -32767, -32767, -32767, -65, -32767, -65, -32767, -32767, -32767, 114, -32767, -32767, 
  /* 状态 136 */
  -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -47, 112, -32767, 115, 127, 110, 126, 116, -47, 121, -47, -32767, -32767, -47, -47, 111, 113, -47, -32767, -47, -32767, -32767, -32767, -32767, -32767, -32767, -47, -32767, -47, -32767, -32767, -32767, 114, -32767, -32767, 
  /* 状态 137 */
  -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -48, 112, -32767, 115, 127, 110, 126, 116, -48, 121, -48, -32767, -32767, -48, -48, 111, 113, -48, -32767, -48, -32767, -32767, -32767, -32767, -32767, -32767, -48, -32767, -48, -32767, -32767, -32767, 114, -32767, -32767, 
  /* 状态 138 */
  -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -49, 112, -32767, 115, 127, 110, 126, 116, -49, 121, -49, -32767, -32767, -49, -49, 111, 113, -49, -32767, -49, -32767, -32767, -32767, -32767, -32767, -32767, -49, -32767, -49, -32767, -32767, -32767, 114, -32767, -32767, 
  /* 状态 139 */
  -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -50, 112, -32767, 115, 127, 110, 126, 116, 118, 121, 120, -32767, -32767, -50, 119, 111, 113, 122, -32767, 125, -32767, -32767, -32767, -32767, -32767, -32767, 124, -32767, -50, -32767, -32767, -32767, 114, -32767, -32767, 
  /* 状态 140 */
  -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -51, 112, -32767, 115, 127, 110, -51, -51, -51, 121, -51, -32767, -32767, -51, -51, 111, 113, -51, -32767, -51, -32767, -32767, -32767, -32767, -32767, -32767, -51, -32767, -51, -32767, -32767, -32767, 114, -32767, -32767, 
  /* 状态 141 */
  -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -53, -53, -32767, -53, -53, -53, -53, -53, -53, -53, -53, -32767, -32767, -53, -53, 111, 113, -53, -32767, -53, -32767, -32767, -32767, -32767, -32767, -32767, -53, -32767, -53, -32767, -32767, -32767, -53, -32767, -32767, 
  /* 状态 142 */
  -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -54, -54, -32767, -54, -54, -54, -54, -54, -54, -54, -54, -32767, -32767, -54, -54, 111, 113, -54, -32767, -54, -32767, -32767, -32767, -32767, -32767, -32767, -54, -32767, -54, -32767, -32767, -32767, -54, -32767, -32767, 
  /* 状态 143 */
  -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -69, -69, -32767, -69, -69, -69, -69, -69, -69, -69, -69, -32767, -32767, -69, -69, 111, 113, -69, -32767, -69, -32767, -32767, -32767, -32767, -32767, -32767, -69, -32767, -69, -32767, -32767, -32767, -69, -32767, -32767, 
  /* 状态 144 */
  -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -55, -55, -32767, -55, -55, -55, -55, -55, -55, -55, -55, -32767, -32767, -55, -55, 111, 113, -55, -32767, -55, -32767, -32767, -32767, -32767, -32767, -32767, -55, -32767, -55, -32767, -32767, -32767, -55, -32767, -32767, 
  /* 状态 145 */
  -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -68, -68, -32767, -68, -68, -68, -68, -68, -68, -68, -68, -32767, -32767, -68, -68, 111, 113, -68, -32767, -68, -32767, -32767, -32767, -32767, -32767, -32767, -68, -32767, -68, -32767, -32767, -32767, -68, -32767, -32767, 
  /* 状态 146 */
  -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -70, 112, -32767, 115, 127, -70, -70, -70, -70, 121, -70, -32767, -32767, -70, -70, 111, 113, -70, -32767, -70, -32767, -32767, -32767, -32767, -32767, -32767, -70, -32767, -70, -32767, -32767, -32767, 114, -32767, -32767, 
  /* 状态 147 */
  -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -56, -56, -32767, -56, -56, -56, -56, -56, -56, -56, -56, -32767, -32767, -56, -56, -56, -56, -56, -32767, -56, -32767, -32767, -32767, -32767, -32767, -32767, -56, -32767, -56, -32767, -32767, -32767, -56, -32767, -32767, 
  /* 状态 148 */
  -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 149, 112, -32767, 115, 127, 110, 126, 116, 118, 121, 120, -32767, -32767, 117, 119, 111, 113, 122, -32767, 125, -32767, -32767, -32767, -32767, -32767, -32767, 124, -32767, 128, -32767, -32767, -32767, 114, -32767, -32767, 
  /* 状态 149 */
  -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -60, -60, -32767, -60, -60, -60, -60, -60, -60, -60, -60, -32767, -32767, -60, -60, -60, -60, -60, -32767, -60, -32767, -32767, -32767, -32767, -32767, -32767, -60, -32767, -60, -32767, -32767, -32767, -60, -32767, -32767, 
  /* 状态 150 */
  -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -57, -57, -32767, -57, -57, -57, -57, -57, -57, -57, -57, -32767, -32767, -57, -57, -57, -57, -57, -32767, -57, -32767, -32767, -32767, -32767, -32767, -32767, -57, -32767, -57, -32767, -32767, -32767, -57, -32767, -32767, 
  /* 状态 151 */
  -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -58, -58, -32767, -58, -58, -58, -58, -58, -58, -58, -58, -32767, -32767, -58, -58, -58, -58, -58, -32767, -58, -32767, -32767, -32767, -32767, -32767, -32767, -58, -32767, -58, -32767, -32767, -32767, -58, -32767, -32767, 
  /* 状态 152 */
  -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -59, -59, -32767, -59, -59, -59, -59, -59, -59, -59, -59, -32767, -32767, -59, -59, -59, -59, -59, -32767, -59, -32767, -32767, -32767, -32767, -32767, -32767, -59, -32767, -59, -32767, -32767, -32767, -59, -32767, -32767, 
  /* 状态 153 */
  -32767, 222, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -76, -32767, -32767, -32767, -32767, -32767, 220, 219, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 218, 215, 214, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 216, -32767, 223, 213, 
  /* 状态 154 */
  -32767, 163, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 162, 161, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 160, 157, 156, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 159, -32767, 164, 155, 
  /* 状态 155 */
  -32767, -32767, -32767, -32767, -32767, -32767, -72, -32767, -32767, -32767, -72, -32767, -72, -72, -72, -72, -72, -72, -72, -72, -32767, -32767, -72, -72, -72, -72, -72, -32767, -72, -32767, -32767, -32767, -32767, -32767, -32767, -72, -32767, -72, -32767, -32767, -32767, -72, -32767, -32767, 
  /* 状态 156 */
  -32767, -32767, -32767, -32767, -32767, -32767, -71, -32767, -32767, -32767, -71, -32767, -71, -71, -71, -71, -71, -71, -71, -71, -32767, -32767, -71, -71, -71, -71, -71, -32767, -71, -32767, -32767, -32767, -32767, -32767, -32767, -71, -32767, -71, -32767, -32767, -32767, -71, -32767, -32767, 
  /* 状态 157 */
  -32767, 163, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 162, 161, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 160, 157, 156, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 159, -32767, 164, 155, 
  /* 状态 158 */
  -32767, -32767, -32767, -32767, -32767, -32767, -64, -32767, -32767, -32767, -64, -32767, -64, -64, -64, -64, -64, -64, -64, -64, -32767, -32767, -64, -64, -64, -64, -64, -32767, -64, -32767, -32767, -32767, -32767, -32767, -32767, -64, -32767, -64, -32767, -32767, -32767, -64, -32767, -32767, 
  /* 状态 159 */
  -32767, 209, -32767, 210, -32767, -32767, -61, -32767, -32767, -32767, -61, -32767, -61, -61, -61, -61, -61, -61, -61, -61, -32767, -32767, -61, -61, -61, -61, -61, -32767, -61, -32767, -32767, -32767, -32767, -32767, -32767, -61, -32767, -61, -32767, -32767, -32767, -61, -32767, -32767, 
  /* 状态 160 */
  -32767, 163, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 162, 161, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 160, 157, 156, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 159, -32767, 164, 155, 
  /* 状态 161 */
  -32767, 163, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 162, 161, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 160, 157, 156, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 159, -32767, 164, 155, 
  /* 状态 162 */
  -32767, 163, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 162, 161, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 160, 157, 156, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 159, -32767, 164, 155, 
  /* 状态 163 */
  -32767, 107, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 106, 105, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 104, 101, 100, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 103, -32767, 108, 99, 
  /* 状态 164 */
  -32767, 163, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 162, 161, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 160, 157, 156, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 159, -32767, 164, 155, 
  /* 状态 165 */
  -32767, -32767, -32767, -32767, -32767, -32767, 184, -32767, -32767, -32767, 168, -32767, 171, 182, 166, 181, 173, 174, 177, 176, -32767, -32767, 172, 175, 167, 169, 178, -32767, 180, -32767, -32767, -32767, -32767, -32767, -32767, 179, -32767, 183, -32767, -32767, -32767, 170, -32767, -32767, 
  /* 状态 166 */
  -32767, 163, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 162, 161, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 160, 157, 156, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 159, -32767, 164, 155, 
  /* 状态 167 */
  -32767, 163, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 162, 161, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 160, 157, 156, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 159, -32767, 164, 155, 
  /* 状态 168 */
  -32767, 163, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 162, 161, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 160, 157, 156, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 159, -32767, 164, 155, 
  /* 状态 169 */
  -32767, 163, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 162, 161, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 160, 157, 156, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 159, -32767, 164, 155, 
  /* 状态 170 */
  -32767, 163, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 162, 161, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 160, 157, 156, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 159, -32767, 164, 155, 
  /* 状态 171 */
  -32767, 163, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 162, 161, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 160, 157, 156, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 159, -32767, 164, 155, 
  /* 状态 172 */
  -32767, 163, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 162, 161, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 160, 157, 156, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 159, -32767, 164, 155, 
  /* 状态 173 */
  -32767, 163, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 162, 161, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 160, 157, 156, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 159, -32767, 164, 155, 
  /* 状态 174 */
  -32767, 163, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 162, 161, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 160, 157, 156, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 159, -32767, 164, 155, 
  /* 状态 175 */
  -32767, 163, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 162, 161, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 160, 157, 156, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 159, -32767, 164, 155, 
  /* 状态 176 */
  -32767, 163, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 162, 161, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 160, 157, 156, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 159, -32767, 164, 155, 
  /* 状态 177 */
  -32767, 163, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 162, 161, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 160, 157, 156, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 159, -32767, 164, 155, 
  /* 状态 178 */
  -32767, 163, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 162, 161, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 160, 157, 156, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 159, -32767, 164, 155, 
  /* 状态 179 */
  -32767, 163, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 162, 161, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 160, 157, 156, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 159, -32767, 164, 155, 
  /* 状态 180 */
  -32767, 163, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 162, 161, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 160, 157, 156, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 159, -32767, 164, 155, 
  /* 状态 181 */
  -32767, 163, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 162, 161, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 160, 157, 156, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 159, -32767, 164, 155, 
  /* 状态 182 */
  -32767, 163, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 162, 161, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 160, 157, 156, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 159, -32767, 164, 155, 
  /* 状态 183 */
  -32767, 163, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 162, 161, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 160, 157, 156, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 159, -32767, 164, 155, 
  /* 状态 184 */
  -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -62, -62, -32767, -62, -62, -62, -62, -62, -62, -62, -62, -32767, -32767, -62, -62, -62, -62, -62, -32767, -62, -32767, -32767, -32767, -32767, -32767, -32767, -62, -32767, -62, -32767, -32767, -32767, -62, -32767, -32767, 
  /* 状态 185 */
  -32767, -32767, -32767, -32767, -32767, -32767, -43, -32767, -32767, -32767, 168, -32767, 171, 182, 166, 181, 173, 174, 177, 176, -32767, -32767, 172, 175, 167, 169, 178, -32767, 180, -32767, -32767, -32767, -32767, -32767, -32767, 179, -32767, -43, -32767, -32767, -32767, 170, -32767, -32767, 
  /* 状态 186 */
  -32767, -32767, -32767, -32767, -32767, -32767, -66, -32767, -32767, -32767, 168, -32767, 171, -66, -66, -66, -66, -66, -66, -66, -32767, -32767, -66, -66, 167, 169, -66, -32767, -66, -32767, -32767, -32767, -32767, -32767, -32767, -66, -32767, -66, -32767, -32767, -32767, 170, -32767, -32767, 
  /* 状态 187 */
  -32767, -32767, -32767, -32767, -32767, -32767, -52, -32767, -32767, -32767, 168, -32767, 171, 182, 166, -52, -52, -52, 177, -52, -32767, -32767, -52, -52, 167, 169, -52, -32767, -52, -32767, -32767, -32767, -32767, -32767, -32767, -52, -32767, -52, -32767, -32767, -32767, 170, -32767, -32767, 
  /* 状态 188 */
  -32767, -32767, -32767, -32767, -32767, -32767, -44, -32767, -32767, -32767, 168, -32767, 171, 182, 166, 181, 173, -44, 177, -44, -32767, -32767, -44, -44, 167, 169, -44, -32767, -44, -32767, -32767, -32767, -32767, -32767, -32767, -44, -32767, -44, -32767, -32767, -32767, 170, -32767, -32767, 
  /* 状态 189 */
  -32767, -32767, -32767, -32767, -32767, -32767, -46, -32767, -32767, -32767, 168, -32767, 171, 182, 166, 181, 173, -46, 177, -46, -32767, -32767, -46, -46, 167, 169, -46, -32767, -46, -32767, -32767, -32767, -32767, -32767, -32767, -46, -32767, -46, -32767, -32767, -32767, 170, -32767, -32767, 
  /* 状态 190 */
  -32767, -32767, -32767, -32767, -32767, -32767, -45, -32767, -32767, -32767, 168, -32767, 171, 182, 166, 181, 173, -45, 177, -45, -32767, -32767, -45, -45, 167, 169, -45, -32767, -45, -32767, -32767, -32767, -32767, -32767, -32767, -45, -32767, -45, -32767, -32767, -32767, 170, -32767, -32767, 
  /* 状态 191 */
  -32767, -32767, -32767, -32767, -32767, -32767, -65, -32767, -32767, -32767, 168, -32767, 171, -65, -65, -65, -65, -65, -65, -65, -32767, -32767, -65, -65, 167, 169, -65, -32767, -65, -32767, -32767, -32767, -32767, -32767, -32767, -65, -32767, -65, -32767, -32767, -32767, 170, -32767, -32767, 
  /* 状态 192 */
  -32767, -32767, -32767, -32767, -32767, -32767, -47, -32767, -32767, -32767, 168, -32767, 171, 182, 166, 181, 173, -47, 177, -47, -32767, -32767, -47, -47, 167, 169, -47, -32767, -47, -32767, -32767, -32767, -32767, -32767, -32767, -47, -32767, -47, -32767, -32767, -32767, 170, -32767, -32767, 
  /* 状态 193 */
  -32767, -32767, -32767, -32767, -32767, -32767, -48, -32767, -32767, -32767, 168, -32767, 171, 182, 166, 181, 173, -48, 177, -48, -32767, -32767, -48, -48, 167, 169, -48, -32767, -48, -32767, -32767, -32767, -32767, -32767, -32767, -48, -32767, -48, -32767, -32767, -32767, 170, -32767, -32767, 
  /* 状态 194 */
  -32767, -32767, -32767, -32767, -32767, -32767, -49, -32767, -32767, -32767, 168, -32767, 171, 182, 166, 181, 173, -49, 177, -49, -32767, -32767, -49, -49, 167, 169, -49, -32767, -49, -32767, -32767, -32767, -32767, -32767, -32767, -49, -32767, -49, -32767, -32767, -32767, 170, -32767, -32767, 
  /* 状态 195 */
  -32767, -32767, -32767, -32767, -32767, -32767, -51, -32767, -32767, -32767, 168, -32767, 171, 182, 166, -51, -51, -51, 177, -51, -32767, -32767, -51, -51, 167, 169, -51, -32767, -51, -32767, -32767, -32767, -32767, -32767, -32767, -51, -32767, -51, -32767, -32767, -32767, 170, -32767, -32767, 
  /* 状态 196 */
  -32767, -32767, -32767, -32767, -32767, -32767, -50, -32767, -32767, -32767, 168, -32767, 171, 182, 166, 181, 173, 174, 177, 176, -32767, -32767, -50, 175, 167, 169, 178, -32767, 180, -32767, -32767, -32767, -32767, -32767, -32767, 179, -32767, -50, -32767, -32767, -32767, 170, -32767, -32767, 
  /* 状态 197 */
  -32767, -32767, -32767, -32767, -32767, -32767, -53, -32767, -32767, -32767, -53, -32767, -53, -53, -53, -53, -53, -53, -53, -53, -32767, -32767, -53, -53, 167, 169, -53, -32767, -53, -32767, -32767, -32767, -32767, -32767, -32767, -53, -32767, -53, -32767, -32767, -32767, -53, -32767, -32767, 
  /* 状态 198 */
  -32767, -32767, -32767, -32767, -32767, -32767, -54, -32767, -32767, -32767, -54, -32767, -54, -54, -54, -54, -54, -54, -54, -54, -32767, -32767, -54, -54, 167, 169, -54, -32767, -54, -32767, -32767, -32767, -32767, -32767, -32767, -54, -32767, -54, -32767, -32767, -32767, -54, -32767, -32767, 
  /* 状态 199 */
  -32767, -32767, -32767, -32767, -32767, -32767, -69, -32767, -32767, -32767, -69, -32767, -69, -69, -69, -69, -69, -69, -69, -69, -32767, -32767, -69, -69, 167, 169, -69, -32767, -69, -32767, -32767, -32767, -32767, -32767, -32767, -69, -32767, -69, -32767, -32767, -32767, -69, -32767, -32767, 
  /* 状态 200 */
  -32767, -32767, -32767, -32767, -32767, -32767, -55, -32767, -32767, -32767, -55, -32767, -55, -55, -55, -55, -55, -55, -55, -55, -32767, -32767, -55, -55, 167, 169, -55, -32767, -55, -32767, -32767, -32767, -32767, -32767, -32767, -55, -32767, -55, -32767, -32767, -32767, -55, -32767, -32767, 
  /* 状态 201 */
  -32767, -32767, -32767, -32767, -32767, -32767, -68, -32767, -32767, -32767, -68, -32767, -68, -68, -68, -68, -68, -68, -68, -68, -32767, -32767, -68, -68, 167, 169, -68, -32767, -68, -32767, -32767, -32767, -32767, -32767, -32767, -68, -32767, -68, -32767, -32767, -32767, -68, -32767, -32767, 
  /* 状态 202 */
  -32767, -32767, -32767, -32767, -32767, -32767, -70, -32767, -32767, -32767, 168, -32767, 171, 182, -70, -70, -70, -70, 177, -70, -32767, -32767, -70, -70, 167, 169, -70, -32767, -70, -32767, -32767, -32767, -32767, -32767, -32767, -70, -32767, -70, -32767, -32767, -32767, 170, -32767, -32767, 
  /* 状态 203 */
  -32767, -32767, -32767, -32767, -32767, -32767, -56, -32767, -32767, -32767, -56, -32767, -56, -56, -56, -56, -56, -56, -56, -56, -32767, -32767, -56, -56, -56, -56, -56, -32767, -56, -32767, -32767, -32767, -32767, -32767, -32767, -56, -32767, -56, -32767, -32767, -32767, -56, -32767, -32767, 
  /* 状态 204 */
  -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 205, 112, -32767, 115, 127, 110, 126, 116, 118, 121, 120, -32767, -32767, 117, 119, 111, 113, 122, -32767, 125, -32767, -32767, -32767, -32767, -32767, -32767, 124, -32767, 128, -32767, -32767, -32767, 114, -32767, -32767, 
  /* 状态 205 */
  -32767, -32767, -32767, -32767, -32767, -32767, -60, -32767, -32767, -32767, -60, -32767, -60, -60, -60, -60, -60, -60, -60, -60, -32767, -32767, -60, -60, -60, -60, -60, -32767, -60, -32767, -32767, -32767, -32767, -32767, -32767, -60, -32767, -60, -32767, -32767, -32767, -60, -32767, -32767, 
  /* 状态 206 */
  -32767, -32767, -32767, -32767, -32767, -32767, -57, -32767, -32767, -32767, -57, -32767, -57, -57, -57, -57, -57, -57, -57, -57, -32767, -32767, -57, -57, -57, -57, -57, -32767, -57, -32767, -32767, -32767, -32767, -32767, -32767, -57, -32767, -57, -32767, -32767, -32767, -57, -32767, -32767, 
  /* 状态 207 */
  -32767, -32767, -32767, -32767, -32767, -32767, -58, -32767, -32767, -32767, -58, -32767, -58, -58, -58, -58, -58, -58, -58, -58, -32767, -32767, -58, -58, -58, -58, -58, -32767, -58, -32767, -32767, -32767, -32767, -32767, -32767, -58, -32767, -58, -32767, -32767, -32767, -58, -32767, -32767, 
  /* 状态 208 */
  -32767, -32767, -32767, -32767, -32767, -32767, -59, -32767, -32767, -32767, -59, -32767, -59, -59, -59, -59, -59, -59, -59, -59, -32767, -32767, -59, -59, -59, -59, -59, -32767, -59, -32767, -32767, -32767, -32767, -32767, -32767, -59, -32767, -59, -32767, -32767, -32767, -59, -32767, -32767, 
  /* 状态 209 */
  -32767, 222, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -76, -32767, -32767, -32767, -32767, -32767, 220, 219, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 218, 215, 214, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 216, -32767, 223, 213, 
  /* 状态 210 */
  -32767, 163, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 162, 161, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 160, 157, 156, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 159, -32767, 164, 155, 
  /* 状态 211 */
  -32767, -32767, -32767, -32767, -32767, -32767, 212, -32767, -32767, -32767, 168, -32767, 171, 182, 166, 181, 173, 174, 177, 176, -32767, -32767, 172, 175, 167, 169, 178, -32767, 180, -32767, -32767, -32767, -32767, -32767, -32767, 179, -32767, 183, -32767, -32767, -32767, 170, -32767, -32767, 
  /* 状态 212 */
  -32767, -32767, -32767, -32767, -32767, -32767, -62, -32767, -32767, -32767, -62, -32767, -62, -62, -62, -62, -62, -62, -62, -62, -32767, -32767, -62, -62, -62, -62, -62, -32767, -62, -32767, -32767, -32767, -32767, -32767, -32767, -62, -32767, -62, -32767, -32767, -32767, -62, -32767, -32767, 
  /* 状态 213 */
  -32767, -32767, -32767, -32767, -72, -32767, -32767, -32767, -32767, -72, -72, -32767, -72, -72, -72, -72, -72, -72, -72, -72, -32767, -32767, -72, -72, -72, -72, -72, -32767, -72, -32767, -32767, -32767, -32767, -32767, -32767, -72, -32767, -72, -32767, -32767, -32767, -72, -32767, -32767, 
  /* 状态 214 */
  -32767, -32767, -32767, -32767, -71, -32767, -32767, -32767, -32767, -71, -71, -32767, -71, -71, -71, -71, -71, -71, -71, -71, -32767, -32767, -71, -71, -71, -71, -71, -32767, -71, -32767, -32767, -32767, -32767, -32767, -32767, -71, -32767, -71, -32767, -32767, -32767, -71, -32767, -32767, 
  /* 状态 215 */
  -32767, 222, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 220, 219, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 218, 215, 214, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 216, -32767, 223, 213, 
  /* 状态 216 */
  -32767, 271, -32767, 272, -61, -32767, -32767, -32767, -32767, -61, -61, -32767, -61, -61, -61, -61, -61, -61, -61, -61, -32767, -32767, -61, -61, -61, -61, -61, -32767, -61, -32767, -32767, -32767, -32767, -32767, -32767, -61, -32767, -61, -32767, -32767, -32767, -61, -32767, -32767, 
  /* 状态 217 */
  -32767, -32767, -32767, -32767, -64, -32767, -32767, -32767, -32767, -64, -64, -32767, -64, -64, -64, -64, -64, -64, -64, -64, -32767, -32767, -64, -64, -64, -64, -64, -32767, -64, -32767, -32767, -32767, -32767, -32767, -32767, -64, -32767, -64, -32767, -32767, -32767, -64, -32767, -32767, 
  /* 状态 218 */
  -32767, 222, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 220, 219, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 218, 215, 214, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 216, -32767, 223, 213, 
  /* 状态 219 */
  -32767, 222, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 220, 219, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 218, 215, 214, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 216, -32767, 223, 213, 
  /* 状态 220 */
  -32767, 222, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 220, 219, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 218, 215, 214, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 216, -32767, 223, 213, 
  /* 状态 221 */
  -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 267, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 
  /* 状态 222 */
  -32767, 107, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 106, 105, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 104, 101, 100, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 103, -32767, 108, 99, 
  /* 状态 223 */
  -32767, 222, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 220, 219, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 218, 215, 214, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 216, -32767, 223, 213, 
  /* 状态 224 */
  -32767, -32767, -32767, -32767, -74, -32767, -32767, -32767, -32767, -74, 230, -32767, 233, 244, 228, 243, 234, 236, 238, 241, -32767, -32767, 235, 237, 229, 231, 239, -32767, 242, -32767, -32767, -32767, -32767, -32767, -32767, 240, -32767, 245, -32767, -32767, -32767, 232, -32767, -32767, 
  /* 状态 225 */
  -32767, -32767, -32767, -32767, 226, -32767, -32767, -32767, -32767, -75, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 
  /* 状态 226 */
  -32767, 222, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 220, 219, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 218, 215, 214, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 216, -32767, 223, 213, 
  /* 状态 227 */
  -32767, -32767, -32767, -32767, -73, -32767, -32767, -32767, -32767, -73, 230, -32767, 233, 244, 228, 243, 234, 236, 238, 241, -32767, -32767, 235, 237, 229, 231, 239, -32767, 242, -32767, -32767, -32767, -32767, -32767, -32767, 240, -32767, 245, -32767, -32767, -32767, 232, -32767, -32767, 
  /* 状态 228 */
  -32767, 222, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 220, 219, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 218, 215, 214, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 216, -32767, 223, 213, 
  /* 状态 229 */
  -32767, 222, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 220, 219, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 218, 215, 214, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 216, -32767, 223, 213, 
  /* 状态 230 */
  -32767, 222, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 220, 219, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 218, 215, 214, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 216, -32767, 223, 213, 
  /* 状态 231 */
  -32767, 222, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 220, 219, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 218, 215, 214, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 216, -32767, 223, 213, 
  /* 状态 232 */
  -32767, 222, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 220, 219, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 218, 215, 214, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 216, -32767, 223, 213, 
  /* 状态 233 */
  -32767, 222, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 220, 219, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 218, 215, 214, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 216, -32767, 223, 213, 
  /* 状态 234 */
  -32767, 222, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 220, 219, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 218, 215, 214, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 216, -32767, 223, 213, 
  /* 状态 235 */
  -32767, 222, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 220, 219, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 218, 215, 214, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 216, -32767, 223, 213, 
  /* 状态 236 */
  -32767, 222, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 220, 219, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 218, 215, 214, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 216, -32767, 223, 213, 
  /* 状态 237 */
  -32767, 222, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 220, 219, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 218, 215, 214, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 216, -32767, 223, 213, 
  /* 状态 238 */
  -32767, 222, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 220, 219, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 218, 215, 214, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 216, -32767, 223, 213, 
  /* 状态 239 */
  -32767, 222, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 220, 219, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 218, 215, 214, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 216, -32767, 223, 213, 
  /* 状态 240 */
  -32767, 222, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 220, 219, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 218, 215, 214, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 216, -32767, 223, 213, 
  /* 状态 241 */
  -32767, 222, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 220, 219, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 218, 215, 214, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 216, -32767, 223, 213, 
  /* 状态 242 */
  -32767, 222, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 220, 219, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 218, 215, 214, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 216, -32767, 223, 213, 
  /* 状态 243 */
  -32767, 222, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 220, 219, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 218, 215, 214, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 216, -32767, 223, 213, 
  /* 状态 244 */
  -32767, 222, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 220, 219, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 218, 215, 214, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 216, -32767, 223, 213, 
  /* 状态 245 */
  -32767, 222, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 220, 219, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 218, 215, 214, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 216, -32767, 223, 213, 
  /* 状态 246 */
  -32767, -32767, -32767, -32767, -43, -32767, -32767, -32767, -32767, -43, 230, -32767, 233, 244, 228, 243, 234, 236, 238, 241, -32767, -32767, 235, 237, 229, 231, 239, -32767, 242, -32767, -32767, -32767, -32767, -32767, -32767, 240, -32767, -43, -32767, -32767, -32767, 232, -32767, -32767, 
  /* 状态 247 */
  -32767, -32767, -32767, -32767, -66, -32767, -32767, -32767, -32767, -66, 230, -32767, 233, -66, -66, -66, -66, -66, -66, -66, -32767, -32767, -66, -66, 229, 231, -66, -32767, -66, -32767, -32767, -32767, -32767, -32767, -32767, -66, -32767, -66, -32767, -32767, -32767, 232, -32767, -32767, 
  /* 状态 248 */
  -32767, -32767, -32767, -32767, -52, -32767, -32767, -32767, -32767, -52, 230, -32767, 233, 244, 228, -52, -52, -52, 238, -52, -32767, -32767, -52, -52, 229, 231, -52, -32767, -52, -32767, -32767, -32767, -32767, -32767, -32767, -52, -32767, -52, -32767, -32767, -32767, 232, -32767, -32767, 
  /* 状态 249 */
  -32767, -32767, -32767, -32767, -44, -32767, -32767, -32767, -32767, -44, 230, -32767, 233, 244, 228, 243, 234, -44, 238, -44, -32767, -32767, -44, -44, 229, 231, -44, -32767, -44, -32767, -32767, -32767, -32767, -32767, -32767, -44, -32767, -44, -32767, -32767, -32767, 232, -32767, -32767, 
  /* 状态 250 */
  -32767, -32767, -32767, -32767, -47, -32767, -32767, -32767, -32767, -47, 230, -32767, 233, 244, 228, 243, 234, -47, 238, -47, -32767, -32767, -47, -47, 229, 231, -47, -32767, -47, -32767, -32767, -32767, -32767, -32767, -32767, -47, -32767, -47, -32767, -32767, -32767, 232, -32767, -32767, 
  /* 状态 251 */
  -32767, -32767, -32767, -32767, -46, -32767, -32767, -32767, -32767, -46, 230, -32767, 233, 244, 228, 243, 234, -46, 238, -46, -32767, -32767, -46, -46, 229, 231, -46, -32767, -46, -32767, -32767, -32767, -32767, -32767, -32767, -46, -32767, -46, -32767, -32767, -32767, 232, -32767, -32767, 
  /* 状态 252 */
  -32767, -32767, -32767, -32767, -45, -32767, -32767, -32767, -32767, -45, 230, -32767, 233, 244, 228, 243, 234, -45, 238, -45, -32767, -32767, -45, -45, 229, 231, -45, -32767, -45, -32767, -32767, -32767, -32767, -32767, -32767, -45, -32767, -45, -32767, -32767, -32767, 232, -32767, -32767, 
  /* 状态 253 */
  -32767, -32767, -32767, -32767, -65, -32767, -32767, -32767, -32767, -65, 230, -32767, 233, -65, -65, -65, -65, -65, -65, -65, -32767, -32767, -65, -65, 229, 231, -65, -32767, -65, -32767, -32767, -32767, -32767, -32767, -32767, -65, -32767, -65, -32767, -32767, -32767, 232, -32767, -32767, 
  /* 状态 254 */
  -32767, -32767, -32767, -32767, -48, -32767, -32767, -32767, -32767, -48, 230, -32767, 233, 244, 228, 243, 234, -48, 238, -48, -32767, -32767, -48, -48, 229, 231, -48, -32767, -48, -32767, -32767, -32767, -32767, -32767, -32767, -48, -32767, -48, -32767, -32767, -32767, 232, -32767, -32767, 
  /* 状态 255 */
  -32767, -32767, -32767, -32767, -49, -32767, -32767, -32767, -32767, -49, 230, -32767, 233, 244, 228, 243, 234, -49, 238, -49, -32767, -32767, -49, -49, 229, 231, -49, -32767, -49, -32767, -32767, -32767, -32767, -32767, -32767, -49, -32767, -49, -32767, -32767, -32767, 232, -32767, -32767, 
  /* 状态 256 */
  -32767, -32767, -32767, -32767, -50, -32767, -32767, -32767, -32767, -50, 230, -32767, 233, 244, 228, 243, 234, 236, 238, 241, -32767, -32767, -50, 237, 229, 231, 239, -32767, 242, -32767, -32767, -32767, -32767, -32767, -32767, 240, -32767, -50, -32767, -32767, -32767, 232, -32767, -32767, 
  /* 状态 257 */
  -32767, -32767, -32767, -32767, -51, -32767, -32767, -32767, -32767, -51, 230, -32767, 233, 244, 228, -51, -51, -51, 238, -51, -32767, -32767, -51, -51, 229, 231, -51, -32767, -51, -32767, -32767, -32767, -32767, -32767, -32767, -51, -32767, -51, -32767, -32767, -32767, 232, -32767, -32767, 
  /* 状态 258 */
  -32767, -32767, -32767, -32767, -53, -32767, -32767, -32767, -32767, -53, -53, -32767, -53, -53, -53, -53, -53, -53, -53, -53, -32767, -32767, -53, -53, 229, 231, -53, -32767, -53, -32767, -32767, -32767, -32767, -32767, -32767, -53, -32767, -53, -32767, -32767, -32767, -53, -32767, -32767, 
  /* 状态 259 */
  -32767, -32767, -32767, -32767, -54, -32767, -32767, -32767, -32767, -54, -54, -32767, -54, -54, -54, -54, -54, -54, -54, -54, -32767, -32767, -54, -54, 229, 231, -54, -32767, -54, -32767, -32767, -32767, -32767, -32767, -32767, -54, -32767, -54, -32767, -32767, -32767, -54, -32767, -32767, 
  /* 状态 260 */
  -32767, -32767, -32767, -32767, -69, -32767, -32767, -32767, -32767, -69, -69, -32767, -69, -69, -69, -69, -69, -69, -69, -69, -32767, -32767, -69, -69, 229, 231, -69, -32767, -69, -32767, -32767, -32767, -32767, -32767, -32767, -69, -32767, -69, -32767, -32767, -32767, -69, -32767, -32767, 
  /* 状态 261 */
  -32767, -32767, -32767, -32767, -55, -32767, -32767, -32767, -32767, -55, -55, -32767, -55, -55, -55, -55, -55, -55, -55, -55, -32767, -32767, -55, -55, 229, 231, -55, -32767, -55, -32767, -32767, -32767, -32767, -32767, -32767, -55, -32767, -55, -32767, -32767, -32767, -55, -32767, -32767, 
  /* 状态 262 */
  -32767, -32767, -32767, -32767, -68, -32767, -32767, -32767, -32767, -68, -68, -32767, -68, -68, -68, -68, -68, -68, -68, -68, -32767, -32767, -68, -68, 229, 231, -68, -32767, -68, -32767, -32767, -32767, -32767, -32767, -32767, -68, -32767, -68, -32767, -32767, -32767, -68, -32767, -32767, 
  /* 状态 263 */
  -32767, -32767, -32767, -32767, -70, -32767, -32767, -32767, -32767, -70, 230, -32767, 233, 244, -70, -70, -70, -70, 238, -70, -32767, -32767, -70, -70, 229, 231, -70, -32767, -70, -32767, -32767, -32767, -32767, -32767, -32767, -70, -32767, -70, -32767, -32767, -32767, 232, -32767, -32767, 
  /* 状态 264 */
  -32767, -32767, -32767, -32767, -56, -32767, -32767, -32767, -32767, -56, -56, -32767, -56, -56, -56, -56, -56, -56, -56, -56, -32767, -32767, -56, -56, -56, -56, -56, -32767, -56, -32767, -32767, -32767, -32767, -32767, -32767, -56, -32767, -56, -32767, -32767, -32767, -56, -32767, -32767, 
  /* 状态 265 */
  -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 266, 112, -32767, 115, 127, 110, 126, 116, 118, 121, 120, -32767, -32767, 117, 119, 111, 113, 122, -32767, 125, -32767, -32767, -32767, -32767, -32767, -32767, 124, -32767, 128, -32767, -32767, -32767, 114, -32767, -32767, 
  /* 状态 266 */
  -32767, -32767, -32767, -32767, -60, -32767, -32767, -32767, -32767, -60, -60, -32767, -60, -60, -60, -60, -60, -60, -60, -60, -32767, -32767, -60, -60, -60, -60, -60, -32767, -60, -32767, -32767, -32767, -32767, -32767, -32767, -60, -32767, -60, -32767, -32767, -32767, -60, -32767, -32767, 
  /* 状态 267 */
  -32767, -32767, -32767, -32767, -32767, -32767, -63, -32767, -32767, -32767, -63, -32767, -63, -63, -63, -63, -63, -63, -63, -63, -32767, -32767, -63, -63, -63, -63, -63, -32767, -63, -32767, -32767, -32767, -32767, -32767, -32767, -63, -32767, -63, -32767, -32767, -32767, -63, -32767, -32767, 
  /* 状态 268 */
  -32767, -32767, -32767, -32767, -57, -32767, -32767, -32767, -32767, -57, -57, -32767, -57, -57, -57, -57, -57, -57, -57, -57, -32767, -32767, -57, -57, -57, -57, -57, -32767, -57, -32767, -32767, -32767, -32767, -32767, -32767, -57, -32767, -57, -32767, -32767, -32767, -57, -32767, -32767, 
  /* 状态 269 */
  -32767, -32767, -32767, -32767, -58, -32767, -32767, -32767, -32767, -58, -58, -32767, -58, -58, -58, -58, -58, -58, -58, -58, -32767, -32767, -58, -58, -58, -58, -58, -32767, -58, -32767, -32767, -32767, -32767, -32767, -32767, -58, -32767, -58, -32767, -32767, -32767, -58, -32767, -32767, 
  /* 状态 270 */
  -32767, -32767, -32767, -32767, -59, -32767, -32767, -32767, -32767, -59, -59, -32767, -59, -59, -59, -59, -59, -59, -59, -59, -32767, -32767, -59, -59, -59, -59, -59, -32767, -59, -32767, -32767, -32767, -32767, -32767, -32767, -59, -32767, -59, -32767, -32767, -32767, -59, -32767, -32767, 
  /* 状态 271 */
  -32767, 222, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -76, -32767, -32767, -32767, -32767, -32767, 220, 219, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 218, 215, 214, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 216, -32767, 223, 213, 
  /* 状态 272 */
  -32767, 163, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 162, 161, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 160, 157, 156, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 159, -32767, 164, 155, 
  /* 状态 273 */
  -32767, -32767, -32767, -32767, -32767, -32767, 274, -32767, -32767, -32767, 168, -32767, 171, 182, 166, 181, 173, 174, 177, 176, -32767, -32767, 172, 175, 167, 169, 178, -32767, 180, -32767, -32767, -32767, -32767, -32767, -32767, 179, -32767, 183, -32767, -32767, -32767, 170, -32767, -32767, 
  /* 状态 274 */
  -32767, -32767, -32767, -32767, -62, -32767, -32767, -32767, -32767, -62, -62, -32767, -62, -62, -62, -62, -62, -62, -62, -62, -32767, -32767, -62, -62, -62, -62, -62, -32767, -62, -32767, -32767, -32767, -32767, -32767, -32767, -62, -32767, -62, -32767, -32767, -32767, -62, -32767, -32767, 
  /* 状态 275 */
  -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 276, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 
  /* 状态 276 */
  -32767, -32767, -32767, -32767, -63, -32767, -32767, -32767, -32767, -63, -63, -32767, -63, -63, -63, -63, -63, -63, -63, -63, -32767, -32767, -63, -63, -63, -63, -63, -32767, -63, -32767, -32767, -32767, -32767, -32767, -32767, -63, -32767, -63, -32767, -32767, -32767, -63, -32767, -32767, 
  /* 状态 277 */
  -32767, -32767, -32767, -32767, -67, -32767, -32767, -32767, -32767, -67, -67, -32767, -67, -67, -67, -67, -67, -67, -67, -67, -32767, -32767, -67, -67, -67, -67, -67, -32767, -67, -32767, -32767, -32767, -32767, -32767, -32767, -67, -32767, -67, -32767, -32767, -32767, -67, -32767, -32767, 
  /* 状态 278 */
  -32767, -32767, -32767, -32767, -32767, -32767, -67, -32767, -32767, -32767, -67, -32767, -67, -67, -67, -67, -67, -67, -67, -67, -32767, -32767, -67, -67, -67, -67, -67, -32767, -67, -32767, -32767, -32767, -32767, -32767, -32767, -67, -32767, -67, -32767, -32767, -32767, -67, -32767, -32767, 
  /* 状态 279 */
  -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 280, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 
  /* 状态 280 */
  -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -63, -63, -32767, -63, -63, -63, -63, -63, -63, -63, -63, -32767, -32767, -63, -63, -63, -63, -63, -32767, -63, -32767, -32767, -32767, -32767, -32767, -32767, -63, -32767, -63, -32767, -32767, -32767, -63, -32767, -32767, 
  /* 状态 281 */
  -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -67, -67, -32767, -67, -67, -67, -67, -67, -67, -67, -67, -32767, -32767, -67, -67, -67, -67, -67, -32767, -67, -32767, -32767, -32767, -32767, -32767, -32767, -67, -32767, -67, -32767, -32767, -32767, -67, -32767, -32767, 
  /* 状态 282 */
  -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -57, -57, -57, -57, -57, -57, -57, -57, -57, -57, -32767, -32767, -57, -57, -57, -57, -57, -32767, -57, -32767, -32767, -32767, -32767, -32767, -32767, -57, -32767, -57, -32767, -32767, -32767, -57, -32767, -32767, 
  /* 状态 283 */
  -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -58, -58, -58, -58, -58, -58, -58, -58, -58, -58, -32767, -32767, -58, -58, -58, -58, -58, -32767, -58, -32767, -32767, -32767, -32767, -32767, -32767, -58, -32767, -58, -32767, -32767, -32767, -58, -32767, -32767, 
  /* 状态 284 */
  -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -59, -59, -59, -59, -59, -59, -59, -59, -59, -59, -32767, -32767, -59, -59, -59, -59, -59, -32767, -59, -32767, -32767, -32767, -32767, -32767, -32767, -59, -32767, -59, -32767, -32767, -32767, -59, -32767, -32767, 
  /* 状态 285 */
  -32767, 222, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -76, -32767, -32767, -32767, -32767, -32767, 220, 219, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 218, 215, 214, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 216, -32767, 223, 213, 
  /* 状态 286 */
  -32767, 163, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 162, 161, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 160, 157, 156, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 159, -32767, 164, 155, 
  /* 状态 287 */
  -32767, -32767, -32767, -32767, -32767, -32767, 288, -32767, -32767, -32767, 168, -32767, 171, 182, 166, 181, 173, 174, 177, 176, -32767, -32767, 172, 175, 167, 169, 178, -32767, 180, -32767, -32767, -32767, -32767, -32767, -32767, 179, -32767, 183, -32767, -32767, -32767, 170, -32767, -32767, 
  /* 状态 288 */
  -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -62, -62, -62, -62, -62, -62, -62, -62, -62, -62, -32767, -32767, -62, -62, -62, -62, -62, -32767, -62, -32767, -32767, -32767, -32767, -32767, -32767, -62, -32767, -62, -32767, -32767, -32767, -62, -32767, -32767, 
  /* 状态 289 */
  -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 290, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 
  /* 状态 290 */
  -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -63, -63, -63, -63, -63, -63, -63, -63, -63, -63, -32767, -32767, -63, -63, -63, -63, -63, -32767, -63, -32767, -32767, -32767, -32767, -32767, -32767, -63, -32767, -63, -32767, -32767, -32767, -63, -32767, -32767, 
  /* 状态 291 */
  -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -67, -67, -67, -67, -67, -67, -67, -67, -67, -67, -32767, -32767, -67, -67, -67, -67, -67, -32767, -67, -32767, -32767, -32767, -32767, -32767, -32767, -67, -32767, -67, -32767, -32767, -32767, -67, -32767, -32767, 
  /* 状态 292 */
  -32767, -32767, -32767, -32767, -32767, -32767, 293, -32767, -32767, -32767, 168, -32767, 171, 182, 166, 181, 173, 174, 177, 176, -32767, -32767, 172, 175, 167, 169, 178, -32767, 180, -32767, -32767, -32767, -32767, -32767, -32767, 179, -32767, 183, -32767, -32767, -32767, 170, -32767, -32767, 
  /* 状态 293 */
  -32767, -32767, -32767, -32767, -32767, 294, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 
  /* 状态 294 */
  -32767, 58, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 57, 56, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 55, 52, 51, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 54, -32767, 59, 50, 
  /* 状态 295 */
  -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 63, 296, 66, 78, 61, 77, 67, 69, 73, 71, -32767, -32767, 68, 70, 62, 64, 74, -32767, 76, -32767, -32767, -32767, -32767, -32767, -32767, 75, -32767, 79, -32767, -32767, -32767, 65, -32767, -32767, 
  /* 状态 296 */
  -32767, -32767, -29, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -29, -32767, -29, -29, -32767, -32767, -29, -29, -32767, -29, -32767, -32767, -29, -29, -32767, -32767, -32767, 
  /* 状态 297 */
  -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 298, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 
  /* 状态 298 */
  -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 299, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 
  /* 状态 299 */
  -32767, -32767, -31, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -31, -32767, -31, -31, -32767, -32767, -31, -31, -32767, -31, -32767, -32767, -31, -31, -32767, -32767, -32767, 
  /* 状态 300 */
  -32767, -32767, -32767, -32767, -32767, -72, -32767, -32767, -32767, -32767, -72, -32767, -72, -72, -72, -72, -72, -72, -72, -72, -32767, -32767, -72, -72, -72, -72, -72, -32767, -72, -32767, -32767, -32767, -32767, -32767, -32767, -72, -32767, -72, -32767, -32767, -32767, -72, -32767, -32767, 
  /* 状态 301 */
  -32767, -32767, -32767, -32767, -32767, -71, -32767, -32767, -32767, -32767, -71, -32767, -71, -71, -71, -71, -71, -71, -71, -71, -32767, -32767, -71, -71, -71, -71, -71, -32767, -71, -32767, -32767, -32767, -32767, -32767, -32767, -71, -32767, -71, -32767, -32767, -32767, -71, -32767, -32767, 
  /* 状态 302 */
  -32767, 308, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 307, 306, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 305, 302, 301, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 304, -32767, 309, 300, 
  /* 状态 303 */
  -32767, -32767, -32767, -32767, -32767, -64, -32767, -32767, -32767, -32767, -64, -32767, -64, -64, -64, -64, -64, -64, -64, -64, -32767, -32767, -64, -64, -64, -64, -64, -32767, -64, -32767, -32767, -32767, -32767, -32767, -32767, -64, -32767, -64, -32767, -32767, -32767, -64, -32767, -32767, 
  /* 状态 304 */
  -32767, 356, -32767, 357, -32767, -61, -32767, -32767, -32767, -32767, -61, -32767, -61, -61, -61, -61, -61, -61, -61, -61, -32767, -32767, -61, -61, -61, -61, -61, -32767, -61, -32767, -32767, -32767, -32767, -32767, -32767, -61, -32767, -61, -32767, -32767, -32767, -61, -32767, -32767, 
  /* 状态 305 */
  -32767, 308, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 307, 306, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 305, 302, 301, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 304, -32767, 309, 300, 
  /* 状态 306 */
  -32767, 308, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 307, 306, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 305, 302, 301, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 304, -32767, 309, 300, 
  /* 状态 307 */
  -32767, 308, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 307, 306, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 305, 302, 301, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 304, -32767, 309, 300, 
  /* 状态 308 */
  -32767, 107, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 106, 105, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 104, 101, 100, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 103, -32767, 108, 99, 
  /* 状态 309 */
  -32767, 308, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 307, 306, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 305, 302, 301, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 304, -32767, 309, 300, 
  /* 状态 310 */
  -32767, -32767, -32767, -32767, -32767, 329, -32767, -32767, -32767, -32767, 313, -32767, 316, 327, 311, 326, 317, 319, 324, 322, -32767, -32767, 318, 320, 312, 314, 325, -32767, 321, -32767, -32767, -32767, -32767, -32767, -32767, 323, -32767, 328, -32767, -32767, -32767, 315, -32767, -32767, 
  /* 状态 311 */
  -32767, 308, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 307, 306, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 305, 302, 301, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 304, -32767, 309, 300, 
  /* 状态 312 */
  -32767, 308, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 307, 306, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 305, 302, 301, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 304, -32767, 309, 300, 
  /* 状态 313 */
  -32767, 308, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 307, 306, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 305, 302, 301, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 304, -32767, 309, 300, 
  /* 状态 314 */
  -32767, 308, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 307, 306, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 305, 302, 301, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 304, -32767, 309, 300, 
  /* 状态 315 */
  -32767, 308, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 307, 306, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 305, 302, 301, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 304, -32767, 309, 300, 
  /* 状态 316 */
  -32767, 308, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 307, 306, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 305, 302, 301, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 304, -32767, 309, 300, 
  /* 状态 317 */
  -32767, 308, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 307, 306, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 305, 302, 301, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 304, -32767, 309, 300, 
  /* 状态 318 */
  -32767, 308, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 307, 306, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 305, 302, 301, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 304, -32767, 309, 300, 
  /* 状态 319 */
  -32767, 308, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 307, 306, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 305, 302, 301, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 304, -32767, 309, 300, 
  /* 状态 320 */
  -32767, 308, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 307, 306, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 305, 302, 301, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 304, -32767, 309, 300, 
  /* 状态 321 */
  -32767, 308, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 307, 306, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 305, 302, 301, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 304, -32767, 309, 300, 
  /* 状态 322 */
  -32767, 308, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 307, 306, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 305, 302, 301, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 304, -32767, 309, 300, 
  /* 状态 323 */
  -32767, 308, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 307, 306, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 305, 302, 301, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 304, -32767, 309, 300, 
  /* 状态 324 */
  -32767, 308, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 307, 306, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 305, 302, 301, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 304, -32767, 309, 300, 
  /* 状态 325 */
  -32767, 308, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 307, 306, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 305, 302, 301, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 304, -32767, 309, 300, 
  /* 状态 326 */
  -32767, 308, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 307, 306, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 305, 302, 301, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 304, -32767, 309, 300, 
  /* 状态 327 */
  -32767, 308, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 307, 306, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 305, 302, 301, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 304, -32767, 309, 300, 
  /* 状态 328 */
  -32767, 308, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 307, 306, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 305, 302, 301, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 304, -32767, 309, 300, 
  /* 状态 329 */
  -32767, 58, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 57, 56, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 55, 52, 51, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 54, -32767, 59, 50, 
  /* 状态 330 */
  -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 63, 331, 66, 78, 61, 77, 67, 69, 73, 71, -32767, -32767, 68, 70, 62, 64, 74, -32767, 76, -32767, -32767, -32767, -32767, -32767, -32767, 75, -32767, 79, -32767, -32767, -32767, 65, -32767, -32767, 
  /* 状态 331 */
  -32767, -32767, -30, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -30, -32767, -30, -30, -32767, -32767, -30, -30, -32767, -30, -32767, -32767, -30, -30, -32767, -32767, -32767, 
  /* 状态 332 */
  -32767, -32767, -32767, -32767, -32767, -43, -32767, -32767, -32767, -32767, 313, -32767, 316, 327, 311, 326, 317, 319, 324, 322, -32767, -32767, 318, 320, 312, 314, 325, -32767, 321, -32767, -32767, -32767, -32767, -32767, -32767, 323, -32767, -43, -32767, -32767, -32767, 315, -32767, -32767, 
  /* 状态 333 */
  -32767, -32767, -32767, -32767, -32767, -66, -32767, -32767, -32767, -32767, 313, -32767, 316, -66, -66, -66, -66, -66, -66, -66, -32767, -32767, -66, -66, 312, 314, -66, -32767, -66, -32767, -32767, -32767, -32767, -32767, -32767, -66, -32767, -66, -32767, -32767, -32767, 315, -32767, -32767, 
  /* 状态 334 */
  -32767, -32767, -32767, -32767, -32767, -52, -32767, -32767, -32767, -32767, 313, -32767, 316, 327, 311, -52, -52, -52, 324, -52, -32767, -32767, -52, -52, 312, 314, -52, -32767, -52, -32767, -32767, -32767, -32767, -32767, -32767, -52, -32767, -52, -32767, -32767, -32767, 315, -32767, -32767, 
  /* 状态 335 */
  -32767, -32767, -32767, -32767, -32767, -45, -32767, -32767, -32767, -32767, 313, -32767, 316, 327, 311, 326, 317, -45, 324, -45, -32767, -32767, -45, -45, 312, 314, -45, -32767, -45, -32767, -32767, -32767, -32767, -32767, -32767, -45, -32767, -45, -32767, -32767, -32767, 315, -32767, -32767, 
  /* 状态 336 */
  -32767, -32767, -32767, -32767, -32767, -65, -32767, -32767, -32767, -32767, 313, -32767, 316, -65, -65, -65, -65, -65, -65, -65, -32767, -32767, -65, -65, 312, 314, -65, -32767, -65, -32767, -32767, -32767, -32767, -32767, -32767, -65, -32767, -65, -32767, -32767, -32767, 315, -32767, -32767, 
  /* 状态 337 */
  -32767, -32767, -32767, -32767, -32767, -46, -32767, -32767, -32767, -32767, 313, -32767, 316, 327, 311, 326, 317, -46, 324, -46, -32767, -32767, -46, -46, 312, 314, -46, -32767, -46, -32767, -32767, -32767, -32767, -32767, -32767, -46, -32767, -46, -32767, -32767, -32767, 315, -32767, -32767, 
  /* 状态 338 */
  -32767, -32767, -32767, -32767, -32767, -47, -32767, -32767, -32767, -32767, 313, -32767, 316, 327, 311, 326, 317, -47, 324, -47, -32767, -32767, -47, -47, 312, 314, -47, -32767, -47, -32767, -32767, -32767, -32767, -32767, -32767, -47, -32767, -47, -32767, -32767, -32767, 315, -32767, -32767, 
  /* 状态 339 */
  -32767, -32767, -32767, -32767, -32767, -44, -32767, -32767, -32767, -32767, 313, -32767, 316, 327, 311, 326, 317, -44, 324, -44, -32767, -32767, -44, -44, 312, 314, -44, -32767, -44, -32767, -32767, -32767, -32767, -32767, -32767, -44, -32767, -44, -32767, -32767, -32767, 315, -32767, -32767, 
  /* 状态 340 */
  -32767, -32767, -32767, -32767, -32767, -48, -32767, -32767, -32767, -32767, 313, -32767, 316, 327, 311, 326, 317, -48, 324, -48, -32767, -32767, -48, -48, 312, 314, -48, -32767, -48, -32767, -32767, -32767, -32767, -32767, -32767, -48, -32767, -48, -32767, -32767, -32767, 315, -32767, -32767, 
  /* 状态 341 */
  -32767, -32767, -32767, -32767, -32767, -49, -32767, -32767, -32767, -32767, 313, -32767, 316, 327, 311, 326, 317, -49, 324, -49, -32767, -32767, -49, -49, 312, 314, -49, -32767, -49, -32767, -32767, -32767, -32767, -32767, -32767, -49, -32767, -49, -32767, -32767, -32767, 315, -32767, -32767, 
  /* 状态 342 */
  -32767, -32767, -32767, -32767, -32767, -50, -32767, -32767, -32767, -32767, 313, -32767, 316, 327, 311, 326, 317, 319, 324, 322, -32767, -32767, -50, 320, 312, 314, 325, -32767, 321, -32767, -32767, -32767, -32767, -32767, -32767, 323, -32767, -50, -32767, -32767, -32767, 315, -32767, -32767, 
  /* 状态 343 */
  -32767, -32767, -32767, -32767, -32767, -51, -32767, -32767, -32767, -32767, 313, -32767, 316, 327, 311, -51, -51, -51, 324, -51, -32767, -32767, -51, -51, 312, 314, -51, -32767, -51, -32767, -32767, -32767, -32767, -32767, -32767, -51, -32767, -51, -32767, -32767, -32767, 315, -32767, -32767, 
  /* 状态 344 */
  -32767, -32767, -32767, -32767, -32767, -53, -32767, -32767, -32767, -32767, -53, -32767, -53, -53, -53, -53, -53, -53, -53, -53, -32767, -32767, -53, -53, 312, 314, -53, -32767, -53, -32767, -32767, -32767, -32767, -32767, -32767, -53, -32767, -53, -32767, -32767, -32767, -53, -32767, -32767, 
  /* 状态 345 */
  -32767, -32767, -32767, -32767, -32767, -54, -32767, -32767, -32767, -32767, -54, -32767, -54, -54, -54, -54, -54, -54, -54, -54, -32767, -32767, -54, -54, 312, 314, -54, -32767, -54, -32767, -32767, -32767, -32767, -32767, -32767, -54, -32767, -54, -32767, -32767, -32767, -54, -32767, -32767, 
  /* 状态 346 */
  -32767, -32767, -32767, -32767, -32767, -69, -32767, -32767, -32767, -32767, -69, -32767, -69, -69, -69, -69, -69, -69, -69, -69, -32767, -32767, -69, -69, 312, 314, -69, -32767, -69, -32767, -32767, -32767, -32767, -32767, -32767, -69, -32767, -69, -32767, -32767, -32767, -69, -32767, -32767, 
  /* 状态 347 */
  -32767, -32767, -32767, -32767, -32767, -55, -32767, -32767, -32767, -32767, -55, -32767, -55, -55, -55, -55, -55, -55, -55, -55, -32767, -32767, -55, -55, 312, 314, -55, -32767, -55, -32767, -32767, -32767, -32767, -32767, -32767, -55, -32767, -55, -32767, -32767, -32767, -55, -32767, -32767, 
  /* 状态 348 */
  -32767, -32767, -32767, -32767, -32767, -68, -32767, -32767, -32767, -32767, -68, -32767, -68, -68, -68, -68, -68, -68, -68, -68, -32767, -32767, -68, -68, 312, 314, -68, -32767, -68, -32767, -32767, -32767, -32767, -32767, -32767, -68, -32767, -68, -32767, -32767, -32767, -68, -32767, -32767, 
  /* 状态 349 */
  -32767, -32767, -32767, -32767, -32767, -70, -32767, -32767, -32767, -32767, 313, -32767, 316, 327, -70, -70, -70, -70, 324, -70, -32767, -32767, -70, -70, 312, 314, -70, -32767, -70, -32767, -32767, -32767, -32767, -32767, -32767, -70, -32767, -70, -32767, -32767, -32767, 315, -32767, -32767, 
  /* 状态 350 */
  -32767, -32767, -32767, -32767, -32767, -56, -32767, -32767, -32767, -32767, -56, -32767, -56, -56, -56, -56, -56, -56, -56, -56, -32767, -32767, -56, -56, -56, -56, -56, -32767, -56, -32767, -32767, -32767, -32767, -32767, -32767, -56, -32767, -56, -32767, -32767, -32767, -56, -32767, -32767, 
  /* 状态 351 */
  -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 352, 112, -32767, 115, 127, 110, 126, 116, 118, 121, 120, -32767, -32767, 117, 119, 111, 113, 122, -32767, 125, -32767, -32767, -32767, -32767, -32767, -32767, 124, -32767, 128, -32767, -32767, -32767, 114, -32767, -32767, 
  /* 状态 352 */
  -32767, -32767, -32767, -32767, -32767, -60, -32767, -32767, -32767, -32767, -60, -32767, -60, -60, -60, -60, -60, -60, -60, -60, -32767, -32767, -60, -60, -60, -60, -60, -32767, -60, -32767, -32767, -32767, -32767, -32767, -32767, -60, -32767, -60, -32767, -32767, -32767, -60, -32767, -32767, 
  /* 状态 353 */
  -32767, -32767, -32767, -32767, -32767, -57, -32767, -32767, -32767, -32767, -57, -32767, -57, -57, -57, -57, -57, -57, -57, -57, -32767, -32767, -57, -57, -57, -57, -57, -32767, -57, -32767, -32767, -32767, -32767, -32767, -32767, -57, -32767, -57, -32767, -32767, -32767, -57, -32767, -32767, 
  /* 状态 354 */
  -32767, -32767, -32767, -32767, -32767, -58, -32767, -32767, -32767, -32767, -58, -32767, -58, -58, -58, -58, -58, -58, -58, -58, -32767, -32767, -58, -58, -58, -58, -58, -32767, -58, -32767, -32767, -32767, -32767, -32767, -32767, -58, -32767, -58, -32767, -32767, -32767, -58, -32767, -32767, 
  /* 状态 355 */
  -32767, -32767, -32767, -32767, -32767, -59, -32767, -32767, -32767, -32767, -59, -32767, -59, -59, -59, -59, -59, -59, -59, -59, -32767, -32767, -59, -59, -59, -59, -59, -32767, -59, -32767, -32767, -32767, -32767, -32767, -32767, -59, -32767, -59, -32767, -32767, -32767, -59, -32767, -32767, 
  /* 状态 356 */
  -32767, 222, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -76, -32767, -32767, -32767, -32767, -32767, 220, 219, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 218, 215, 214, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 216, -32767, 223, 213, 
  /* 状态 357 */
  -32767, 163, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 162, 161, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 160, 157, 156, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 159, -32767, 164, 155, 
  /* 状态 358 */
  -32767, -32767, -32767, -32767, -32767, -32767, 359, -32767, -32767, -32767, 168, -32767, 171, 182, 166, 181, 173, 174, 177, 176, -32767, -32767, 172, 175, 167, 169, 178, -32767, 180, -32767, -32767, -32767, -32767, -32767, -32767, 179, -32767, 183, -32767, -32767, -32767, 170, -32767, -32767, 
  /* 状态 359 */
  -32767, -32767, -32767, -32767, -32767, -62, -32767, -32767, -32767, -32767, -62, -32767, -62, -62, -62, -62, -62, -62, -62, -62, -32767, -32767, -62, -62, -62, -62, -62, -32767, -62, -32767, -32767, -32767, -32767, -32767, -32767, -62, -32767, -62, -32767, -32767, -32767, -62, -32767, -32767, 
  /* 状态 360 */
  -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 361, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 
  /* 状态 361 */
  -32767, -32767, -32767, -32767, -32767, -63, -32767, -32767, -32767, -32767, -63, -32767, -63, -63, -63, -63, -63, -63, -63, -63, -32767, -32767, -63, -63, -63, -63, -63, -32767, -63, -32767, -32767, -32767, -32767, -32767, -32767, -63, -32767, -63, -32767, -32767, -32767, -63, -32767, -32767, 
  /* 状态 362 */
  -32767, -32767, -32767, -32767, -32767, -67, -32767, -32767, -32767, -32767, -67, -32767, -67, -67, -67, -67, -67, -67, -67, -67, -32767, -32767, -67, -67, -67, -67, -67, -32767, -67, -32767, -32767, -32767, -32767, -32767, -32767, -67, -32767, -67, -32767, -32767, -32767, -67, -32767, -32767, 
  /* 状态 363 */
  -32767, 107, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 106, 105, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 104, 101, 100, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 103, -32767, 108, 99, 
  /* 状态 364 */
  -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 365, 112, -32767, 115, 127, 110, 126, 116, 118, 121, 120, -32767, -32767, 117, 119, 111, 113, 122, -32767, 125, -32767, -32767, -32767, -32767, -32767, -32767, 124, -32767, 128, -32767, -32767, -32767, 114, -32767, -32767, 
  /* 状态 365 */
  -32767, -32767, 372, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 366, -32767, 370, 374, -32767, -32767, 367, 368, -32767, 369, -32767, -32767, -32767, 378, -32767, -32767, -32767, 
  /* 状态 366 */
  -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 417, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 
  /* 状态 367 */
  -32767, 58, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 414, -32767, -32767, -32767, 57, 56, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 55, 52, 51, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 54, -32767, 59, 50, 
  /* 状态 368 */
  -32767, 410, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 
  /* 状态 369 */
  -32767, 404, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 
  /* 状态 370 */
  -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 403, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 
  /* 状态 371 */
  -32767, -32767, -21, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -21, -32767, -32767, -32767, -32767, -32767, -32767, -21, -32767, -21, -21, -32767, -32767, -21, -21, -32767, -21, -32767, -32767, -21, -21, -32767, -32767, -32767, 
  /* 状态 372 */
  -32767, -32767, -20, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -20, -32767, -20, -20, -32767, -32767, -20, -20, -32767, -20, -32767, -32767, -20, -20, -32767, -32767, -32767, 
  /* 状态 373 */
  -32767, -32767, -39, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 399, -32767, -32767, -32767, -32767, -32767, -32767, -39, -32767, -39, -39, -32767, -32767, -39, -39, -32767, -39, -32767, -32767, -39, -39, -32767, -32767, -32767, 
  /* 状态 374 */
  -32767, 308, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 307, 306, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 305, 302, 301, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 304, -32767, 309, 300, 
  /* 状态 375 */
  -32767, -32767, -23, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -23, -32767, -32767, -32767, -32767, -32767, -32767, -23, -32767, -23, -23, -32767, -32767, -23, -23, -32767, -23, -32767, -32767, -23, -23, -32767, -32767, -32767, 
  /* 状态 376 */
  -32767, -32767, -25, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -25, -32767, -32767, -32767, -32767, -32767, -32767, -25, -32767, -25, -25, -32767, -32767, -25, -25, -32767, -25, -32767, -32767, -25, -25, -32767, -32767, -32767, 
  /* 状态 377 */
  -32767, -32767, -24, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -24, -32767, -32767, -32767, -32767, -32767, -32767, -24, -32767, -24, -24, -32767, -32767, -24, -24, -32767, -24, -32767, -32767, -24, -24, -32767, -32767, -32767, 
  /* 状态 378 */
  -32767, 382, -32767, 383, -32767, 384, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 
  /* 状态 379 */
  -32767, -32767, -27, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -27, -32767, -32767, -32767, -32767, -32767, -32767, -27, -32767, -27, -27, -32767, -32767, -27, -27, -32767, -27, -32767, -32767, -27, -27, -32767, -32767, -32767, 
  /* 状态 380 */
  -32767, -32767, -26, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -26, -32767, -32767, -32767, -32767, -32767, -32767, -26, -32767, -26, -26, -32767, -32767, -26, -26, -32767, -26, -32767, -32767, -26, -26, -32767, -32767, -32767, 
  /* 状态 381 */
  -32767, -32767, -22, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -22, -32767, -32767, -32767, -32767, -32767, -32767, -22, -32767, -22, -22, -32767, -32767, -22, -22, -32767, -22, -32767, -32767, -22, -22, -32767, -32767, -32767, 
  /* 状态 382 */
  -32767, 222, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -76, -32767, -32767, -32767, -32767, -32767, 220, 219, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 218, 215, 214, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 216, -32767, 223, 213, 
  /* 状态 383 */
  -32767, 163, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 162, 161, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 160, 157, 156, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 159, -32767, 164, 155, 
  /* 状态 384 */
  -32767, 58, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 57, 56, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 55, 52, 51, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 54, -32767, 59, 50, 
  /* 状态 385 */
  -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 63, 386, 66, 78, 61, 77, 67, 69, 73, 71, -32767, -32767, 68, 70, 62, 64, 74, -32767, 76, -32767, -32767, -32767, -32767, -32767, -32767, 75, -32767, 79, -32767, -32767, -32767, 65, -32767, -32767, 
  /* 状态 386 */
  -32767, -32767, -28, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -28, -32767, -32767, -32767, -32767, -32767, -32767, -28, -32767, -28, -28, -32767, -32767, -28, -28, -32767, -28, -32767, -32767, -28, -28, -32767, -32767, -32767, 
  /* 状态 387 */
  -32767, -32767, -32767, -32767, -32767, -32767, 388, -32767, -32767, -32767, 168, -32767, 171, 182, 166, 181, 173, 174, 177, 176, -32767, -32767, 172, 175, 167, 169, 178, -32767, 180, -32767, -32767, -32767, -32767, -32767, -32767, 179, -32767, 183, -32767, -32767, -32767, 170, -32767, -32767, 
  /* 状态 388 */
  -32767, -32767, -32767, -32767, -32767, 389, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 
  /* 状态 389 */
  -32767, 58, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 57, 56, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 55, 52, 51, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 54, -32767, 59, 50, 
  /* 状态 390 */
  -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 63, 391, 66, 78, 61, 77, 67, 69, 73, 71, -32767, -32767, 68, 70, 62, 64, 74, -32767, 76, -32767, -32767, -32767, -32767, -32767, -32767, 75, -32767, 79, -32767, -32767, -32767, 65, -32767, -32767, 
  /* 状态 391 */
  -32767, -32767, -29, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -29, -32767, -32767, -32767, -32767, -32767, -32767, -29, -32767, -29, -29, -32767, -32767, -29, -29, -32767, -29, -32767, -32767, -29, -29, -32767, -32767, -32767, 
  /* 状态 392 */
  -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 393, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 
  /* 状态 393 */
  -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 394, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 
  /* 状态 394 */
  -32767, -32767, -31, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -31, -32767, -32767, -32767, -32767, -32767, -32767, -31, -32767, -31, -31, -32767, -32767, -31, -31, -32767, -31, -32767, -32767, -31, -31, -32767, -32767, -32767, 
  /* 状态 395 */
  -32767, -32767, -32767, -32767, -32767, 396, -32767, -32767, -32767, -32767, 313, -32767, 316, 327, 311, 326, 317, 319, 324, 322, -32767, -32767, 318, 320, 312, 314, 325, -32767, 321, -32767, -32767, -32767, -32767, -32767, -32767, 323, -32767, 328, -32767, -32767, -32767, 315, -32767, -32767, 
  /* 状态 396 */
  -32767, 58, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 57, 56, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 55, 52, 51, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 54, -32767, 59, 50, 
  /* 状态 397 */
  -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 63, 398, 66, 78, 61, 77, 67, 69, 73, 71, -32767, -32767, 68, 70, 62, 64, 74, -32767, 76, -32767, -32767, -32767, -32767, -32767, -32767, 75, -32767, 79, -32767, -32767, -32767, 65, -32767, -32767, 
  /* 状态 398 */
  -32767, -32767, -30, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -30, -32767, -32767, -32767, -32767, -32767, -32767, -30, -32767, -30, -30, -32767, -32767, -30, -30, -32767, -30, -32767, -32767, -30, -30, -32767, -32767, -32767, 
  /* 状态 399 */
  -32767, -32767, 40, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 27, -32767, 39, 32, -32767, -32767, 28, 29, -32767, 30, -32767, -32767, -32767, 35, -32767, -32767, -32767, 
  /* 状态 400 */
  -32767, -32767, -40, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -40, -32767, -40, -40, -32767, -32767, -40, -40, -32767, -40, -32767, -32767, -40, -40, -32767, -32767, -32767, 
  /* 状态 401 */
  -32767, -32767, 40, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 27, -32767, 39, 32, -32767, -32767, 28, 29, -32767, 30, -32767, -32767, 402, 35, -32767, -32767, -32767, 
  /* 状态 402 */
  -32767, -32767, -33, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -33, -32767, -32767, -32767, -32767, -32767, -32767, -33, -32767, -33, -33, -32767, -32767, -33, -33, -32767, -33, -32767, -32767, -33, -33, -32767, -32767, -32767, 
  /* 状态 403 */
  -32767, -32767, -78, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -78, -32767, -32767, -32767, -32767, -32767, -32767, -78, -32767, -78, -78, -32767, -32767, -78, -78, -32767, -78, -32767, -32767, -78, -78, -32767, -32767, -32767, 
  /* 状态 404 */
  -32767, 107, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 106, 105, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 104, 101, 100, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 103, -32767, 108, 99, 
  /* 状态 405 */
  -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 406, 112, -32767, 115, 127, 110, 126, 116, 118, 121, 120, -32767, -32767, 117, 119, 111, 113, 122, -32767, 125, -32767, -32767, -32767, -32767, -32767, -32767, 124, -32767, 128, -32767, -32767, -32767, 114, -32767, -32767, 
  /* 状态 406 */
  -32767, -32767, 372, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 366, -32767, 370, 374, -32767, -32767, 367, 368, -32767, 369, -32767, -32767, -32767, 378, -32767, -32767, -32767, 
  /* 状态 407 */
  -32767, -32767, -39, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 408, -32767, -32767, -32767, -32767, -32767, -32767, -39, -32767, -39, -39, -32767, -32767, -39, -39, -32767, -39, -32767, -32767, -39, -39, -32767, -32767, -32767, 
  /* 状态 408 */
  -32767, -32767, 372, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 366, -32767, 370, 374, -32767, -32767, 367, 368, -32767, 369, -32767, -32767, -32767, 378, -32767, -32767, -32767, 
  /* 状态 409 */
  -32767, -32767, -40, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -40, -32767, -32767, -32767, -32767, -32767, -32767, -40, -32767, -40, -40, -32767, -32767, -40, -40, -32767, -40, -32767, -32767, -40, -40, -32767, -32767, -32767, 
  /* 状态 410 */
  -32767, 107, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 106, 105, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 104, 101, 100, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 103, -32767, 108, 99, 
  /* 状态 411 */
  -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 412, 112, -32767, 115, 127, 110, 126, 116, 118, 121, 120, -32767, -32767, 117, 119, 111, 113, 122, -32767, 125, -32767, -32767, -32767, -32767, -32767, -32767, 124, -32767, 128, -32767, -32767, -32767, 114, -32767, -32767, 
  /* 状态 412 */
  -32767, -32767, 372, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 366, -32767, 370, 374, -32767, -32767, 367, 368, -32767, 369, -32767, -32767, -32767, 378, -32767, -32767, -32767, 
  /* 状态 413 */
  -32767, -32767, -32, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32, -32767, -32767, -32767, -32767, -32767, -32767, -32, -32767, -32, -32, -32767, -32767, -32, -32, -32767, -32, -32767, -32767, -32, -32, -32767, -32767, -32767, 
  /* 状态 414 */
  -32767, -32767, -41, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -41, -32767, -32767, -32767, -32767, -32767, -32767, -41, -32767, -41, -41, -32767, -32767, -41, -41, -32767, -41, -32767, -32767, -41, -41, -32767, -32767, -32767, 
  /* 状态 415 */
  -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 63, 416, 66, 78, 61, 77, 67, 69, 73, 71, -32767, -32767, 68, 70, 62, 64, 74, -32767, 76, -32767, -32767, -32767, -32767, -32767, -32767, 75, -32767, 79, -32767, -32767, -32767, 65, -32767, -32767, 
  /* 状态 416 */
  -32767, -32767, -42, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -42, -32767, -32767, -32767, -32767, -32767, -32767, -42, -32767, -42, -42, -32767, -32767, -42, -42, -32767, -42, -32767, -32767, -42, -42, -32767, -32767, -32767, 
  /* 状态 417 */
  -32767, -32767, -77, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -77, -32767, -32767, -32767, -32767, -32767, -32767, -77, -32767, -77, -77, -32767, -32767, -77, -77, -32767, -77, -32767, -32767, -77, -77, -32767, -32767, -32767, 
  /* 状态 418 */
  -32767, 107, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 106, 105, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 104, 101, 100, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 103, -32767, 108, 99, 
  /* 状态 419 */
  -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 420, 112, -32767, 115, 127, 110, 126, 116, 118, 121, 120, -32767, -32767, 117, 119, 111, 113, 122, -32767, 125, -32767, -32767, -32767, -32767, -32767, -32767, 124, -32767, 128, -32767, -32767, -32767, 114, -32767, -32767, 
  /* 状态 420 */
  -32767, -32767, 40, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 27, -32767, 39, 32, -32767, -32767, 28, 29, -32767, 30, -32767, -32767, -32767, 35, -32767, -32767, -32767, 
  /* 状态 421 */
  -32767, -32767, -32, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32, -32767, -32, -32, -32767, -32767, -32, -32, -32767, -32, -32767, -32767, -32, -32, -32767, -32767, -32767, 
  /* 状态 422 */
  -32767, -32767, -41, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -41, -32767, -41, -41, -32767, -32767, -41, -41, -32767, -41, -32767, -32767, -41, -41, -32767, -32767, -32767, 
  /* 状态 423 */
  -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 63, 424, 66, 78, 61, 77, 67, 69, 73, 71, -32767, -32767, 68, 70, 62, 64, 74, -32767, 76, -32767, -32767, -32767, -32767, -32767, -32767, 75, -32767, 79, -32767, -32767, -32767, 65, -32767, -32767, 
  /* 状态 424 */
  -32767, -32767, -42, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -42, -32767, -42, -42, -32767, -32767, -42, -42, -32767, -42, -32767, -32767, -42, -42, -32767, -32767, -32767, 
  /* 状态 425 */
  -32767, -32767, -77, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -77, -32767, -77, -77, -32767, -32767, -77, -77, -32767, -77, -32767, -32767, -77, -77, -32767, -32767, -32767, 
  /* 状态 426 */
  -32767, -32767, -32767, 427, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 428, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 
  /* 状态 427 */
  -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 429, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 430, 
  /* 状态 428 */
  -32767, -32767, -37, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -37, -32767, -32767, -32767, -32767, -32767, -37, -32767, -37, -37, -32767, -32767, -37, -37, -32767, -37, -32767, -37, -37, -37, -32767, -32767, -32767, 
  /* 状态 429 */
  -32767, -32767, -32767, -32767, -32767, -32767, -71, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 
  /* 状态 430 */
  -32767, -32767, -32767, -32767, -32767, -32767, -72, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 
  /* 状态 431 */
  -32767, -32767, -32767, -32767, -32767, -32767, 432, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 
  /* 状态 432 */
  -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 433, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 
  /* 状态 433 */
  -32767, -32767, -38, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -38, -32767, -32767, -32767, -32767, -32767, -38, -32767, -38, -38, -32767, -32767, -38, -38, -32767, -38, -32767, -38, -38, -38, -32767, -32767, -32767, 
  /* 状态 434 */
  -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 1, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 2, -32767, -32767, -32767, -32767, -32767, 
  /* 状态 435 */
  -32767, -32767, -32767, -32767, -15, -32767, -32767, -32767, -32767, -15, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 
  /* 状态 436 */
  -32767, -32767, -32767, 437, -17, -32767, -32767, -32767, -32767, -17, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 
  /* 状态 437 */
  -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 429, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 430, 
  /* 状态 438 */
  -32767, -32767, -32767, -32767, -32767, -32767, 439, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 
  /* 状态 439 */
  -32767, -32767, -32767, -32767, -18, -32767, -32767, -32767, -32767, -18, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 
  /* 状态 440 */
  -32767, -32767, -32767, -32767, -32767, -32767, 441, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 
  /* 状态 441 */
  -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 442, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, 
  /* 状态 442 */
  -8, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -8, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -8, -32767, -32767, -32767, -32767, -32767, 
};

/* Token 名称表 */
static const char* yytname[] = {
  "$end",
  "$",
  "'('",
  "'{'",
  "'['",
  "','",
  "'='",
  "']'",
  "MPR",
  "UMINUS",
  "')'",
  "'%'",
  "';'",
  "'*'",
  "'^'",
  "'|'",
  "'-'",
  "'+'",
  "'>'",
  "'&'",
  "'<'",
  "ELSE",
  "INT",
  "AND",
  "GE",
  "LSHIFT",
  "RSHIFT",
  "NE",
  "CONTINUE",
  "EQ",
  "BREAK",
  "'$'",
  "'~'",
  "DECNUM",
  "RETURN",
  "WHILE",
  "LE",
  "IF",
  "OR",
  "VOID",
  "'}'",
  "IDENT",
  "'/'",
  "'!'",
  "HEXNUM"
};

static const short yygoto[] = {
  /* 状态 0 */
  -1, -1, -1, -1, -1, -1, 7, 8, -1, -1, 5, -1, -1, -1, -1, -1, -1, -1, 4, -1, -1, -1, 3, 6, -1, 
  /* 状态 1 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 2 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 3 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 4 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 5 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 6 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 7 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 8 */
  -1, -1, -1, -1, -1, -1, 9, -1, -1, -1, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 3, 6, -1, 
  /* 状态 9 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 10 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 11 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 440, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 12 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 16, 15, 18, -1, -1, -1, -1, 14, -1, -1, 
  /* 状态 13 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 14 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 15 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 16 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 17 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 18 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 19 */
  -1, -1, -1, -1, 22, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 20 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 21 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 23, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 22 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 23 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 25, -1, -1, -1, -1, -1, -1, -1, 26, 24, -1, -1, 
  /* 状态 24 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 25 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 26 */
  -1, -1, 38, 36, -1, 37, -1, -1, -1, 41, -1, 33, -1, -1, -1, -1, -1, -1, -1, 34, 31, -1, -1, -1, 42, 
  /* 状态 27 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 28 */
  -1, -1, -1, -1, -1, -1, -1, -1, 423, -1, -1, -1, 53, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 29 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 30 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 31 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 32 */
  -1, -1, -1, -1, -1, -1, -1, -1, 310, -1, -1, -1, 303, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 33 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 34 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 35 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 36 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 37 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 38 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 39 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 40 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 44, -1, -1, -1, 
  /* 状态 41 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 42 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 43 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 44 */
  -1, -1, 38, 36, -1, 37, -1, -1, -1, 41, -1, 33, -1, -1, -1, -1, -1, -1, -1, 34, 31, -1, -1, -1, 42, 
  /* 状态 45 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 46 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 47 */
  225, 297, -1, -1, -1, -1, -1, -1, 224, -1, -1, -1, 217, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 48 */
  -1, -1, -1, -1, -1, -1, -1, -1, 292, -1, -1, -1, 158, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 49 */
  -1, -1, -1, -1, -1, -1, -1, -1, 60, -1, -1, -1, 53, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 50 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 51 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 52 */
  -1, -1, -1, -1, -1, -1, -1, -1, 291, -1, -1, -1, 53, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 53 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 54 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 55 */
  -1, -1, -1, -1, -1, -1, -1, -1, 284, -1, -1, -1, 53, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 56 */
  -1, -1, -1, -1, -1, -1, -1, -1, 283, -1, -1, -1, 53, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 57 */
  -1, -1, -1, -1, -1, -1, -1, -1, 282, -1, -1, -1, 53, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 58 */
  -1, -1, -1, -1, -1, -1, -1, -1, 109, -1, -1, -1, 102, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 59 */
  -1, -1, -1, -1, -1, -1, -1, -1, 98, -1, -1, -1, 53, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 60 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 61 */
  -1, -1, -1, -1, -1, -1, -1, -1, 97, -1, -1, -1, 53, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 62 */
  -1, -1, -1, -1, -1, -1, -1, -1, 96, -1, -1, -1, 53, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 63 */
  -1, -1, -1, -1, -1, -1, -1, -1, 95, -1, -1, -1, 53, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 64 */
  -1, -1, -1, -1, -1, -1, -1, -1, 94, -1, -1, -1, 53, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 65 */
  -1, -1, -1, -1, -1, -1, -1, -1, 93, -1, -1, -1, 53, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 66 */
  -1, -1, -1, -1, -1, -1, -1, -1, 92, -1, -1, -1, 53, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 67 */
  -1, -1, -1, -1, -1, -1, -1, -1, 91, -1, -1, -1, 53, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 68 */
  -1, -1, -1, -1, -1, -1, -1, -1, 90, -1, -1, -1, 53, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 69 */
  -1, -1, -1, -1, -1, -1, -1, -1, 89, -1, -1, -1, 53, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 70 */
  -1, -1, -1, -1, -1, -1, -1, -1, 88, -1, -1, -1, 53, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 71 */
  -1, -1, -1, -1, -1, -1, -1, -1, 87, -1, -1, -1, 53, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 72 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 73 */
  -1, -1, -1, -1, -1, -1, -1, -1, 86, -1, -1, -1, 53, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 74 */
  -1, -1, -1, -1, -1, -1, -1, -1, 85, -1, -1, -1, 53, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 75 */
  -1, -1, -1, -1, -1, -1, -1, -1, 84, -1, -1, -1, 53, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 76 */
  -1, -1, -1, -1, -1, -1, -1, -1, 83, -1, -1, -1, 53, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 77 */
  -1, -1, -1, -1, -1, -1, -1, -1, 82, -1, -1, -1, 53, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 78 */
  -1, -1, -1, -1, -1, -1, -1, -1, 81, -1, -1, -1, 53, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 79 */
  -1, -1, -1, -1, -1, -1, -1, -1, 80, -1, -1, -1, 53, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 80 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 81 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 82 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 83 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 84 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 85 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 86 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 87 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 88 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 89 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 90 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 91 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 92 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 93 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 94 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 95 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 96 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 97 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 98 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 99 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 100 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 101 */
  -1, -1, -1, -1, -1, -1, -1, -1, 281, -1, -1, -1, 102, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 102 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 103 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 104 */
  -1, -1, -1, -1, -1, -1, -1, -1, 152, -1, -1, -1, 102, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 105 */
  -1, -1, -1, -1, -1, -1, -1, -1, 151, -1, -1, -1, 102, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 106 */
  -1, -1, -1, -1, -1, -1, -1, -1, 150, -1, -1, -1, 102, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 107 */
  -1, -1, -1, -1, -1, -1, -1, -1, 148, -1, -1, -1, 102, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 108 */
  -1, -1, -1, -1, -1, -1, -1, -1, 147, -1, -1, -1, 102, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 109 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 110 */
  -1, -1, -1, -1, -1, -1, -1, -1, 146, -1, -1, -1, 102, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 111 */
  -1, -1, -1, -1, -1, -1, -1, -1, 145, -1, -1, -1, 102, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 112 */
  -1, -1, -1, -1, -1, -1, -1, -1, 144, -1, -1, -1, 102, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 113 */
  -1, -1, -1, -1, -1, -1, -1, -1, 143, -1, -1, -1, 102, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 114 */
  -1, -1, -1, -1, -1, -1, -1, -1, 142, -1, -1, -1, 102, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 115 */
  -1, -1, -1, -1, -1, -1, -1, -1, 141, -1, -1, -1, 102, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 116 */
  -1, -1, -1, -1, -1, -1, -1, -1, 140, -1, -1, -1, 102, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 117 */
  -1, -1, -1, -1, -1, -1, -1, -1, 139, -1, -1, -1, 102, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 118 */
  -1, -1, -1, -1, -1, -1, -1, -1, 138, -1, -1, -1, 102, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 119 */
  -1, -1, -1, -1, -1, -1, -1, -1, 137, -1, -1, -1, 102, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 120 */
  -1, -1, -1, -1, -1, -1, -1, -1, 136, -1, -1, -1, 102, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 121 */
  -1, -1, -1, -1, -1, -1, -1, -1, 135, -1, -1, -1, 102, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 122 */
  -1, -1, -1, -1, -1, -1, -1, -1, 134, -1, -1, -1, 102, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 123 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 124 */
  -1, -1, -1, -1, -1, -1, -1, -1, 133, -1, -1, -1, 102, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 125 */
  -1, -1, -1, -1, -1, -1, -1, -1, 132, -1, -1, -1, 102, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 126 */
  -1, -1, -1, -1, -1, -1, -1, -1, 131, -1, -1, -1, 102, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 127 */
  -1, -1, -1, -1, -1, -1, -1, -1, 130, -1, -1, -1, 102, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 128 */
  -1, -1, -1, -1, -1, -1, -1, -1, 129, -1, -1, -1, 102, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 129 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 130 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 131 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 132 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 133 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 134 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 135 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 136 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 137 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 138 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 139 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 140 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 141 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 142 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 143 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 144 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 145 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 146 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 147 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 148 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 149 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 150 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 151 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 152 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 153 */
  225, 279, -1, -1, -1, -1, -1, -1, 224, -1, -1, -1, 217, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 154 */
  -1, -1, -1, -1, -1, -1, -1, -1, 165, -1, -1, -1, 158, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 155 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 156 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 157 */
  -1, -1, -1, -1, -1, -1, -1, -1, 278, -1, -1, -1, 158, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 158 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 159 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 160 */
  -1, -1, -1, -1, -1, -1, -1, -1, 208, -1, -1, -1, 158, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 161 */
  -1, -1, -1, -1, -1, -1, -1, -1, 207, -1, -1, -1, 158, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 162 */
  -1, -1, -1, -1, -1, -1, -1, -1, 206, -1, -1, -1, 158, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 163 */
  -1, -1, -1, -1, -1, -1, -1, -1, 204, -1, -1, -1, 102, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 164 */
  -1, -1, -1, -1, -1, -1, -1, -1, 203, -1, -1, -1, 158, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 165 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 166 */
  -1, -1, -1, -1, -1, -1, -1, -1, 202, -1, -1, -1, 158, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 167 */
  -1, -1, -1, -1, -1, -1, -1, -1, 201, -1, -1, -1, 158, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 168 */
  -1, -1, -1, -1, -1, -1, -1, -1, 200, -1, -1, -1, 158, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 169 */
  -1, -1, -1, -1, -1, -1, -1, -1, 199, -1, -1, -1, 158, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 170 */
  -1, -1, -1, -1, -1, -1, -1, -1, 198, -1, -1, -1, 158, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 171 */
  -1, -1, -1, -1, -1, -1, -1, -1, 197, -1, -1, -1, 158, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 172 */
  -1, -1, -1, -1, -1, -1, -1, -1, 196, -1, -1, -1, 158, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 173 */
  -1, -1, -1, -1, -1, -1, -1, -1, 195, -1, -1, -1, 158, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 174 */
  -1, -1, -1, -1, -1, -1, -1, -1, 194, -1, -1, -1, 158, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 175 */
  -1, -1, -1, -1, -1, -1, -1, -1, 193, -1, -1, -1, 158, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 176 */
  -1, -1, -1, -1, -1, -1, -1, -1, 192, -1, -1, -1, 158, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 177 */
  -1, -1, -1, -1, -1, -1, -1, -1, 191, -1, -1, -1, 158, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 178 */
  -1, -1, -1, -1, -1, -1, -1, -1, 190, -1, -1, -1, 158, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 179 */
  -1, -1, -1, -1, -1, -1, -1, -1, 189, -1, -1, -1, 158, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 180 */
  -1, -1, -1, -1, -1, -1, -1, -1, 188, -1, -1, -1, 158, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 181 */
  -1, -1, -1, -1, -1, -1, -1, -1, 187, -1, -1, -1, 158, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 182 */
  -1, -1, -1, -1, -1, -1, -1, -1, 186, -1, -1, -1, 158, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 183 */
  -1, -1, -1, -1, -1, -1, -1, -1, 185, -1, -1, -1, 158, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 184 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 185 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 186 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 187 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 188 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 189 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 190 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 191 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 192 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 193 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 194 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 195 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 196 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 197 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 198 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 199 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 200 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 201 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 202 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 203 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 204 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 205 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 206 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 207 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 208 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 209 */
  225, 221, -1, -1, -1, -1, -1, -1, 224, -1, -1, -1, 217, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 210 */
  -1, -1, -1, -1, -1, -1, -1, -1, 211, -1, -1, -1, 158, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 211 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 212 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 213 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 214 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 215 */
  -1, -1, -1, -1, -1, -1, -1, -1, 277, -1, -1, -1, 217, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 216 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 217 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 218 */
  -1, -1, -1, -1, -1, -1, -1, -1, 270, -1, -1, -1, 217, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 219 */
  -1, -1, -1, -1, -1, -1, -1, -1, 269, -1, -1, -1, 217, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 220 */
  -1, -1, -1, -1, -1, -1, -1, -1, 268, -1, -1, -1, 217, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 221 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 222 */
  -1, -1, -1, -1, -1, -1, -1, -1, 265, -1, -1, -1, 102, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 223 */
  -1, -1, -1, -1, -1, -1, -1, -1, 264, -1, -1, -1, 217, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 224 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 225 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 226 */
  -1, -1, -1, -1, -1, -1, -1, -1, 227, -1, -1, -1, 217, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 227 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 228 */
  -1, -1, -1, -1, -1, -1, -1, -1, 263, -1, -1, -1, 217, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 229 */
  -1, -1, -1, -1, -1, -1, -1, -1, 262, -1, -1, -1, 217, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 230 */
  -1, -1, -1, -1, -1, -1, -1, -1, 261, -1, -1, -1, 217, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 231 */
  -1, -1, -1, -1, -1, -1, -1, -1, 260, -1, -1, -1, 217, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 232 */
  -1, -1, -1, -1, -1, -1, -1, -1, 259, -1, -1, -1, 217, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 233 */
  -1, -1, -1, -1, -1, -1, -1, -1, 258, -1, -1, -1, 217, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 234 */
  -1, -1, -1, -1, -1, -1, -1, -1, 257, -1, -1, -1, 217, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 235 */
  -1, -1, -1, -1, -1, -1, -1, -1, 256, -1, -1, -1, 217, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 236 */
  -1, -1, -1, -1, -1, -1, -1, -1, 255, -1, -1, -1, 217, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 237 */
  -1, -1, -1, -1, -1, -1, -1, -1, 254, -1, -1, -1, 217, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 238 */
  -1, -1, -1, -1, -1, -1, -1, -1, 253, -1, -1, -1, 217, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 239 */
  -1, -1, -1, -1, -1, -1, -1, -1, 252, -1, -1, -1, 217, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 240 */
  -1, -1, -1, -1, -1, -1, -1, -1, 251, -1, -1, -1, 217, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 241 */
  -1, -1, -1, -1, -1, -1, -1, -1, 250, -1, -1, -1, 217, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 242 */
  -1, -1, -1, -1, -1, -1, -1, -1, 249, -1, -1, -1, 217, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 243 */
  -1, -1, -1, -1, -1, -1, -1, -1, 248, -1, -1, -1, 217, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 244 */
  -1, -1, -1, -1, -1, -1, -1, -1, 247, -1, -1, -1, 217, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 245 */
  -1, -1, -1, -1, -1, -1, -1, -1, 246, -1, -1, -1, 217, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 246 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 247 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 248 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 249 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 250 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 251 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 252 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 253 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 254 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 255 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 256 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 257 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 258 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 259 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 260 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 261 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 262 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 263 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 264 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 265 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 266 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 267 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 268 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 269 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 270 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 271 */
  225, 275, -1, -1, -1, -1, -1, -1, 224, -1, -1, -1, 217, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 272 */
  -1, -1, -1, -1, -1, -1, -1, -1, 273, -1, -1, -1, 158, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 273 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 274 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 275 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 276 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 277 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 278 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 279 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 280 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 281 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 282 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 283 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 284 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 285 */
  225, 289, -1, -1, -1, -1, -1, -1, 224, -1, -1, -1, 217, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 286 */
  -1, -1, -1, -1, -1, -1, -1, -1, 287, -1, -1, -1, 158, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 287 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 288 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 289 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 290 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 291 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 292 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 293 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 294 */
  -1, -1, -1, -1, -1, -1, -1, -1, 295, -1, -1, -1, 53, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 295 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 296 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 297 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 298 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 299 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 300 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 301 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 302 */
  -1, -1, -1, -1, -1, -1, -1, -1, 362, -1, -1, -1, 303, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 303 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 304 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 305 */
  -1, -1, -1, -1, -1, -1, -1, -1, 355, -1, -1, -1, 303, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 306 */
  -1, -1, -1, -1, -1, -1, -1, -1, 354, -1, -1, -1, 303, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 307 */
  -1, -1, -1, -1, -1, -1, -1, -1, 353, -1, -1, -1, 303, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 308 */
  -1, -1, -1, -1, -1, -1, -1, -1, 351, -1, -1, -1, 102, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 309 */
  -1, -1, -1, -1, -1, -1, -1, -1, 350, -1, -1, -1, 303, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 310 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 311 */
  -1, -1, -1, -1, -1, -1, -1, -1, 349, -1, -1, -1, 303, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 312 */
  -1, -1, -1, -1, -1, -1, -1, -1, 348, -1, -1, -1, 303, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 313 */
  -1, -1, -1, -1, -1, -1, -1, -1, 347, -1, -1, -1, 303, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 314 */
  -1, -1, -1, -1, -1, -1, -1, -1, 346, -1, -1, -1, 303, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 315 */
  -1, -1, -1, -1, -1, -1, -1, -1, 345, -1, -1, -1, 303, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 316 */
  -1, -1, -1, -1, -1, -1, -1, -1, 344, -1, -1, -1, 303, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 317 */
  -1, -1, -1, -1, -1, -1, -1, -1, 343, -1, -1, -1, 303, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 318 */
  -1, -1, -1, -1, -1, -1, -1, -1, 342, -1, -1, -1, 303, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 319 */
  -1, -1, -1, -1, -1, -1, -1, -1, 341, -1, -1, -1, 303, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 320 */
  -1, -1, -1, -1, -1, -1, -1, -1, 340, -1, -1, -1, 303, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 321 */
  -1, -1, -1, -1, -1, -1, -1, -1, 339, -1, -1, -1, 303, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 322 */
  -1, -1, -1, -1, -1, -1, -1, -1, 338, -1, -1, -1, 303, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 323 */
  -1, -1, -1, -1, -1, -1, -1, -1, 337, -1, -1, -1, 303, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 324 */
  -1, -1, -1, -1, -1, -1, -1, -1, 336, -1, -1, -1, 303, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 325 */
  -1, -1, -1, -1, -1, -1, -1, -1, 335, -1, -1, -1, 303, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 326 */
  -1, -1, -1, -1, -1, -1, -1, -1, 334, -1, -1, -1, 303, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 327 */
  -1, -1, -1, -1, -1, -1, -1, -1, 333, -1, -1, -1, 303, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 328 */
  -1, -1, -1, -1, -1, -1, -1, -1, 332, -1, -1, -1, 303, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 329 */
  -1, -1, -1, -1, -1, -1, -1, -1, 330, -1, -1, -1, 53, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 330 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 331 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 332 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 333 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 334 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 335 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 336 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 337 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 338 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 339 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 340 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 341 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 342 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 343 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 344 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 345 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 346 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 347 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 348 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 349 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 350 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 351 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 352 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 353 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 354 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 355 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 356 */
  225, 360, -1, -1, -1, -1, -1, -1, 224, -1, -1, -1, 217, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 357 */
  -1, -1, -1, -1, -1, -1, -1, -1, 358, -1, -1, -1, 158, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 358 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 359 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 360 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 361 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 362 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 363 */
  -1, -1, -1, -1, -1, -1, -1, -1, 364, -1, -1, -1, 102, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 364 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 365 */
  -1, -1, 381, 379, -1, 380, -1, -1, -1, 371, -1, 375, -1, -1, -1, -1, -1, -1, -1, 376, 373, -1, -1, -1, 377, 
  /* 状态 366 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 367 */
  -1, -1, -1, -1, -1, -1, -1, -1, 415, -1, -1, -1, 53, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 368 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 369 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 370 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 371 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 372 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 401, -1, -1, -1, 
  /* 状态 373 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 374 */
  -1, -1, -1, -1, -1, -1, -1, -1, 395, -1, -1, -1, 303, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 375 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 376 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 377 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 378 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 379 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 380 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 381 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 382 */
  225, 392, -1, -1, -1, -1, -1, -1, 224, -1, -1, -1, 217, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 383 */
  -1, -1, -1, -1, -1, -1, -1, -1, 387, -1, -1, -1, 158, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 384 */
  -1, -1, -1, -1, -1, -1, -1, -1, 385, -1, -1, -1, 53, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 385 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 386 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 387 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 388 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 389 */
  -1, -1, -1, -1, -1, -1, -1, -1, 390, -1, -1, -1, 53, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 390 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 391 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 392 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 393 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 394 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 395 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 396 */
  -1, -1, -1, -1, -1, -1, -1, -1, 397, -1, -1, -1, 53, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 397 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 398 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 399 */
  -1, -1, 38, 36, -1, 37, -1, -1, -1, 41, -1, 33, -1, -1, -1, -1, -1, -1, -1, 34, 400, -1, -1, -1, 42, 
  /* 状态 400 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 401 */
  -1, -1, 38, 36, -1, 37, -1, -1, -1, 41, -1, 33, -1, -1, -1, -1, -1, -1, -1, 34, 31, -1, -1, -1, 42, 
  /* 状态 402 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 403 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 404 */
  -1, -1, -1, -1, -1, -1, -1, -1, 405, -1, -1, -1, 102, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 405 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 406 */
  -1, -1, 381, 379, -1, 380, -1, -1, -1, 371, -1, 375, -1, -1, -1, -1, -1, -1, -1, 376, 407, -1, -1, -1, 377, 
  /* 状态 407 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 408 */
  -1, -1, 381, 379, -1, 380, -1, -1, -1, 371, -1, 375, -1, -1, -1, -1, -1, -1, -1, 376, 409, -1, -1, -1, 377, 
  /* 状态 409 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 410 */
  -1, -1, -1, -1, -1, -1, -1, -1, 411, -1, -1, -1, 102, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 411 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 412 */
  -1, -1, 381, 379, -1, 380, -1, -1, -1, 371, -1, 375, -1, -1, -1, -1, -1, -1, -1, 376, 413, -1, -1, -1, 377, 
  /* 状态 413 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 414 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 415 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 416 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 417 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 418 */
  -1, -1, -1, -1, -1, -1, -1, -1, 419, -1, -1, -1, 102, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 419 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 420 */
  -1, -1, 38, 36, -1, 37, -1, -1, -1, 41, -1, 33, -1, -1, -1, -1, -1, -1, -1, 34, 421, -1, -1, -1, 42, 
  /* 状态 421 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 422 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 423 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 424 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 425 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 426 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 427 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 431, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 428 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 429 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 430 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 431 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 432 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 433 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 434 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 435, -1, -1, -1, -1, -1, -1, 14, -1, -1, 
  /* 状态 435 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 436 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 437 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 438, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 438 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 439 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 440 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 441 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 状态 442 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
};

/* 每条产生式左部的非终结符索引 */
static const short yyr1[] = {
  0, 62, 51, 51, 50, 50, 67, 67, 66, 66, 54, 54, 61, 61, 60, 60, 59, 59, 65, 65, 64, 64, 64, 64, 64, 64, 64, 53, 53, 53, 53, 68, 46, 48, 58, 58, 57, 57, 55, 55, 63, 63, 52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 56, 56, 44, 44, 45, 45, 49, 47, 
};

/* 每条产生式右部的符号数量 */
static const short yyr2[] = {
  1, 1, 2, 1, 1, 1, 3, 6, 1, 1, 6, 6, 1, 1, 3, 1, 2, 5, 2, 0, 1, 1, 1, 1, 1, 1, 1, 4, 7, 5, 5, 5, 3, 4, 2, 0, 3, 6, 5, 7, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2, 2, 2, 2, 3, 1, 4, 4, 1, 3, 3, 2, 3, 3, 3, 1, 1, 3, 1, 1, 0, 2, 2, 
};

/* 执行规约动作 */
static void yy_reduce(int rule_num, int* top, YYSTYPE* stack, int* state_stack) {
  int symbols_to_pop = yyr2[rule_num];
  printf("  规约详情: 规则%d, 当前栈顶=%d, 弹出%d个符号\n", rule_num, *top, symbols_to_pop);
  YYSTYPE yyval;

  /* 计算栈中元素的位置, $1 是栈中第一个要规约的元素 */
  /* 对应关系: $1 = yyvsp[1], $2 = yyvsp[2], 以此类推 */
  YYSTYPE yyvsp[YYMAXDEPTH + 1]; // 临时数组，下标从1开始
  for (int i = 1; i <= symbols_to_pop; i++) {
    yyvsp[i] = stack[*top - symbols_to_pop + i];
  }

  /* 默认动作: 将$1的值赋给$$ */
  if (symbols_to_pop > 0) {
    yyval = yyvsp[1]; // $$ = $1
  }

  /* 根据规则执行语义动作 */
  printf("  执行语义动作: 规则%d\n", rule_num);
  switch(rule_num) {
    case 0: /* S' -> program  */
      break;
    case 1: /* program -> decl_list  */
      {
        
    yyval.node = create_node(AST_PROGRAM, NULL);
    add_child(yyval.node, yyvsp[1].node);
    ast_root = yyval.node;

        printf("    完成语义动作: %s\n", "program");
      }
      break;
    case 2: /* decl_list -> decl_list decl  */
      {
        
    yyval.node = yyvsp[1].node;
    add_child(yyval.node, yyvsp[2].node);

        printf("    完成语义动作: %s\n", "decl_list");
      }
      break;
    case 3: /* decl_list -> decl  */
      {
        
    yyval.node = create_node(AST_DECL_LIST, NULL);
    add_child(yyval.node, yyvsp[1].node);

        printf("    完成语义动作: %s\n", "decl_list");
      }
      break;
    case 4: /* decl -> var_decl  */
      {
         yyval.node = yyvsp[1].node; 
        printf("    完成语义动作: %s\n", "decl");
      }
      break;
    case 5: /* decl -> fun_decl  */
      {
         yyval.node = yyvsp[1].node; 
        printf("    完成语义动作: %s\n", "decl");
      }
      break;
    case 6: /* var_decl -> type_spec IDENT ';'  */
      {
        
    yyval.node = create_node(AST_VAR_DECL, NULL);
    add_child(yyval.node, yyvsp[1].node);
    add_child(yyval.node, create_node(AST_IDENT, yyvsp[2].str_val));

        printf("    完成语义动作: %s\n", "var_decl");
      }
      break;
    case 7: /* var_decl -> type_spec IDENT '[' int_literal ']' ';'  */
      {
        
    yyval.node = create_node(AST_VAR_DECL, "array");
    add_child(yyval.node, yyvsp[1].node);
    add_child(yyval.node, create_node(AST_IDENT, yyvsp[2].str_val));
    add_child(yyval.node, yyvsp[4].node);

        printf("    完成语义动作: %s\n", "var_decl");
      }
      break;
    case 8: /* type_spec -> VOID  */
      {
         yyval.node = create_node(AST_TYPE_SPEC, "void"); 
        printf("    完成语义动作: %s\n", "type_spec");
      }
      break;
    case 9: /* type_spec -> INT  */
      {
         yyval.node = create_node(AST_TYPE_SPEC, "int"); 
        printf("    完成语义动作: %s\n", "type_spec");
      }
      break;
    case 10: /* fun_decl -> type_spec IDENT '(' params ')' compound_stmt  */
      {
        
    yyval.node = create_node(AST_FUN_DECL, NULL);
    add_child(yyval.node, yyvsp[1].node);
    add_child(yyval.node, create_node(AST_IDENT, yyvsp[2].str_val));
    add_child(yyval.node, yyvsp[4].node);
    add_child(yyval.node, yyvsp[6].node);

        printf("    完成语义动作: %s\n", "fun_decl");
      }
      break;
    case 11: /* fun_decl -> type_spec IDENT '(' params ')' ';'  */
      {
        
    yyval.node = create_node(AST_FUN_DECL, "declaration");
    add_child(yyval.node, yyvsp[1].node);
    add_child(yyval.node, create_node(AST_IDENT, yyvsp[2].str_val));
    add_child(yyval.node, yyvsp[4].node);

        printf("    完成语义动作: %s\n", "fun_decl");
      }
      break;
    case 12: /* params -> param_list  */
      {
         yyval.node = yyvsp[1].node; 
        printf("    完成语义动作: %s\n", "params");
      }
      break;
    case 13: /* params -> VOID  */
      {
         yyval.node = create_node(AST_PARAMS, "void"); 
        printf("    完成语义动作: %s\n", "params");
      }
      break;
    case 14: /* param_list -> param_list ',' param  */
      {
        
    yyval.node = yyvsp[1].node;
    add_child(yyval.node, yyvsp[3].node);

        printf("    完成语义动作: %s\n", "param_list");
      }
      break;
    case 15: /* param_list -> param  */
      {
        
    yyval.node = create_node(AST_PARAM_LIST, NULL);
    add_child(yyval.node, yyvsp[1].node);

        printf("    完成语义动作: %s\n", "param_list");
      }
      break;
    case 16: /* param -> type_spec IDENT  */
      {
        
    yyval.node = create_node(AST_PARAM, NULL);
    add_child(yyval.node, yyvsp[1].node);
    add_child(yyval.node, create_node(AST_IDENT, yyvsp[2].str_val));

        printf("    完成语义动作: %s\n", "param");
      }
      break;
    case 17: /* param -> type_spec IDENT '[' int_literal ']'  */
      {
        
    yyval.node = create_node(AST_PARAM, "array");
    add_child(yyval.node, yyvsp[1].node);
    add_child(yyval.node, create_node(AST_IDENT, yyvsp[2].str_val));
    add_child(yyval.node, yyvsp[4].node);

        printf("    完成语义动作: %s\n", "param");
      }
      break;
    case 18: /* stmt_list -> stmt_list stmt  */
      {
        
    yyval.node = yyvsp[1].node;
    add_child(yyval.node, yyvsp[2].node);

        printf("    完成语义动作: %s\n", "stmt_list");
      }
      break;
    case 19: /* stmt_list -> ε */
      {
        
    yyval.node = create_node(AST_STMT_LIST, NULL);

        printf("    完成语义动作: %s\n", "stmt_list");
      }
      break;
    case 20: /* stmt -> expr_stmt  */
      {
         yyval.node = yyvsp[1].node; 
        printf("    完成语义动作: %s\n", "stmt");
      }
      break;
    case 21: /* stmt -> block_stmt  */
      {
         yyval.node = yyvsp[1].node; 
        printf("    完成语义动作: %s\n", "stmt");
      }
      break;
    case 22: /* stmt -> if_stmt  */
      {
         yyval.node = yyvsp[1].node; 
        printf("    完成语义动作: %s\n", "stmt");
      }
      break;
    case 23: /* stmt -> while_stmt  */
      {
         yyval.node = yyvsp[1].node; 
        printf("    完成语义动作: %s\n", "stmt");
      }
      break;
    case 24: /* stmt -> return_stmt  */
      {
         yyval.node = yyvsp[1].node; 
        printf("    完成语义动作: %s\n", "stmt");
      }
      break;
    case 25: /* stmt -> continue_stmt  */
      {
         yyval.node = yyvsp[1].node; 
        printf("    完成语义动作: %s\n", "stmt");
      }
      break;
    case 26: /* stmt -> break_stmt  */
      {
         yyval.node = yyvsp[1].node; 
        printf("    完成语义动作: %s\n", "stmt");
      }
      break;
    case 27: /* expr_stmt -> IDENT '=' expr ';'  */
      {
        
    yyval.node = create_node(AST_EXPR_STMT, NULL);
    ASTNode* assign = create_node(AST_ASSIGN_EXPR, NULL);
    assign->op = strdup("=");
    add_child(assign, create_node(AST_IDENT, yyvsp[1].str_val));
    add_child(assign, yyvsp[3].node);
    add_child(yyval.node, assign);

        printf("    完成语义动作: %s\n", "expr_stmt");
      }
      break;
    case 28: /* expr_stmt -> IDENT '[' expr ']' '=' expr ';'  */
      {
        
    yyval.node = create_node(AST_EXPR_STMT, NULL);
    ASTNode* assign = create_node(AST_ASSIGN_EXPR, "array");
    assign->op = strdup("=");
    ASTNode* array_access = create_node(AST_ARRAY_ACCESS, NULL);
    add_child(array_access, create_node(AST_IDENT, yyvsp[1].str_val));
    add_child(array_access, yyvsp[3].node);
    add_child(assign, array_access);
    add_child(assign, yyvsp[6].node);
    add_child(yyval.node, assign);

        printf("    完成语义动作: %s\n", "expr_stmt");
      }
      break;
    case 29: /* expr_stmt -> '$' expr '=' expr ';'  */
      {
        
    yyval.node = create_node(AST_EXPR_STMT, "memory_assign");
    ASTNode* assign = create_node(AST_ASSIGN_EXPR, "memory");
    assign->op = strdup("=");
    add_child(assign, yyvsp[2].node);
    add_child(assign, yyvsp[4].node);
    add_child(yyval.node, assign);

        printf("    完成语义动作: %s\n", "expr_stmt");
      }
      break;
    case 30: /* expr_stmt -> IDENT '(' args ')' ';'  */
      {
        
    yyval.node = create_node(AST_EXPR_STMT, NULL);
    ASTNode* call = create_node(AST_FUNC_CALL, yyvsp[1].str_val);
    add_child(call, yyvsp[3].node);
    add_child(yyval.node, call);

        printf("    完成语义动作: %s\n", "expr_stmt");
      }
      break;
    case 31: /* while_stmt -> WHILE '(' expr ')' stmt  */
      {
        
    yyval.node = create_node(AST_WHILE_STMT, NULL);
    add_child(yyval.node, yyvsp[3].node);
    add_child(yyval.node, yyvsp[5].node);

        printf("    完成语义动作: %s\n", "while_stmt");
      }
      break;
    case 32: /* block_stmt -> '{' stmt_list '}'  */
      {
        
    yyval.node = create_node(AST_BLOCK_STMT, NULL);
    add_child(yyval.node, yyvsp[2].node);

        printf("    完成语义动作: %s\n", "block_stmt");
      }
      break;
    case 33: /* compound_stmt -> '{' local_decls stmt_list '}'  */
      {
        
    yyval.node = create_node(AST_COMPOUND_STMT, NULL);
    add_child(yyval.node, yyvsp[2].node);
    add_child(yyval.node, yyvsp[3].node);

        printf("    完成语义动作: %s\n", "compound_stmt");
      }
      break;
    case 34: /* local_decls -> local_decls local_decl  */
      {
        
    yyval.node = yyvsp[1].node;
    add_child(yyval.node, yyvsp[2].node);

        printf("    完成语义动作: %s\n", "local_decls");
      }
      break;
    case 35: /* local_decls -> ε */
      {
        
    yyval.node = create_node(AST_LOCAL_DECLS, NULL);

        printf("    完成语义动作: %s\n", "local_decls");
      }
      break;
    case 36: /* local_decl -> type_spec IDENT ';'  */
      {
        
    yyval.node = create_node(AST_LOCAL_DECL, NULL);
    add_child(yyval.node, yyvsp[1].node);
    add_child(yyval.node, create_node(AST_IDENT, yyvsp[2].str_val));

        printf("    完成语义动作: %s\n", "local_decl");
      }
      break;
    case 37: /* local_decl -> type_spec IDENT '[' int_literal ']' ';'  */
      {
        
    yyval.node = create_node(AST_LOCAL_DECL, "array");
    add_child(yyval.node, yyvsp[1].node);
    add_child(yyval.node, create_node(AST_IDENT, yyvsp[2].str_val));
    add_child(yyval.node, yyvsp[4].node);

        printf("    完成语义动作: %s\n", "local_decl");
      }
      break;
    case 38: /* if_stmt -> IF '(' expr ')' stmt  */
      {
        
    yyval.node = create_node(AST_IF_STMT, NULL);
    add_child(yyval.node, yyvsp[3].node);
    add_child(yyval.node, yyvsp[5].node);

        printf("    完成语义动作: %s\n", "if_stmt");
      }
      break;
    case 39: /* if_stmt -> IF '(' expr ')' stmt ELSE stmt  */
      {
        
    yyval.node = create_node(AST_IF_STMT, "else");
    add_child(yyval.node, yyvsp[3].node);
    add_child(yyval.node, yyvsp[5].node);
    add_child(yyval.node, yyvsp[7].node);

        printf("    完成语义动作: %s\n", "if_stmt");
      }
      break;
    case 40: /* return_stmt -> RETURN ';'  */
      {
        
    yyval.node = create_node(AST_RETURN_STMT, NULL);

        printf("    完成语义动作: %s\n", "return_stmt");
      }
      break;
    case 41: /* return_stmt -> RETURN expr ';'  */
      {
        
    yyval.node = create_node(AST_RETURN_STMT, NULL);
    add_child(yyval.node, yyvsp[2].node);

        printf("    完成语义动作: %s\n", "return_stmt");
      }
      break;
    case 42: /* expr -> expr OR expr  */
      {
        
    yyval.node = create_node(AST_BINARY_EXPR, NULL);
    yyval.node->op = strdup("||");
    add_child(yyval.node, yyvsp[1].node);
    add_child(yyval.node, yyvsp[3].node);

        printf("    完成语义动作: %s\n", "expr");
      }
      break;
    case 43: /* expr -> expr EQ expr  */
      {
        
    yyval.node = create_node(AST_BINARY_EXPR, NULL);
    yyval.node->op = strdup("==");
    add_child(yyval.node, yyvsp[1].node);
    add_child(yyval.node, yyvsp[3].node);

        printf("    完成语义动作: %s\n", "expr");
      }
      break;
    case 44: /* expr -> expr NE expr  */
      {
        
    yyval.node = create_node(AST_BINARY_EXPR, NULL);
    yyval.node->op = strdup("!=");
    add_child(yyval.node, yyvsp[1].node);
    add_child(yyval.node, yyvsp[3].node);

        printf("    完成语义动作: %s\n", "expr");
      }
      break;
    case 45: /* expr -> expr LE expr  */
      {
        
    yyval.node = create_node(AST_BINARY_EXPR, NULL);
    yyval.node->op = strdup("<=");
    add_child(yyval.node, yyvsp[1].node);
    add_child(yyval.node, yyvsp[3].node);

        printf("    完成语义动作: %s\n", "expr");
      }
      break;
    case 46: /* expr -> expr '<' expr  */
      {
        
    yyval.node = create_node(AST_BINARY_EXPR, NULL);
    yyval.node->op = strdup("<");
    add_child(yyval.node, yyvsp[1].node);
    add_child(yyval.node, yyvsp[3].node);

        printf("    完成语义动作: %s\n", "expr");
      }
      break;
    case 47: /* expr -> expr GE expr  */
      {
        
    yyval.node = create_node(AST_BINARY_EXPR, NULL);
    yyval.node->op = strdup(">=");
    add_child(yyval.node, yyvsp[1].node);
    add_child(yyval.node, yyvsp[3].node);

        printf("    完成语义动作: %s\n", "expr");
      }
      break;
    case 48: /* expr -> expr '>' expr  */
      {
        
    yyval.node = create_node(AST_BINARY_EXPR, NULL);
    yyval.node->op = strdup(">");
    add_child(yyval.node, yyvsp[1].node);
    add_child(yyval.node, yyvsp[3].node);

        printf("    完成语义动作: %s\n", "expr");
      }
      break;
    case 49: /* expr -> expr AND expr  */
      {
        
    yyval.node = create_node(AST_BINARY_EXPR, NULL);
    yyval.node->op = strdup("&&");
    add_child(yyval.node, yyvsp[1].node);
    add_child(yyval.node, yyvsp[3].node);

        printf("    完成语义动作: %s\n", "expr");
      }
      break;
    case 50: /* expr -> expr '+' expr  */
      {
        
    yyval.node = create_node(AST_BINARY_EXPR, NULL);
    yyval.node->op = strdup("+");
    add_child(yyval.node, yyvsp[1].node);
    add_child(yyval.node, yyvsp[3].node);

        printf("    完成语义动作: %s\n", "expr");
      }
      break;
    case 51: /* expr -> expr '-' expr  */
      {
        
    yyval.node = create_node(AST_BINARY_EXPR, NULL);
    yyval.node->op = strdup("-");
    add_child(yyval.node, yyvsp[1].node);
    add_child(yyval.node, yyvsp[3].node);

        printf("    完成语义动作: %s\n", "expr");
      }
      break;
    case 52: /* expr -> expr '*' expr  */
      {
        
    yyval.node = create_node(AST_BINARY_EXPR, NULL);
    yyval.node->op = strdup("*");
    add_child(yyval.node, yyvsp[1].node);
    add_child(yyval.node, yyvsp[3].node);

        printf("    完成语义动作: %s\n", "expr");
      }
      break;
    case 53: /* expr -> expr '/' expr  */
      {
        
    yyval.node = create_node(AST_BINARY_EXPR, NULL);
    yyval.node->op = strdup("/");
    add_child(yyval.node, yyvsp[1].node);
    add_child(yyval.node, yyvsp[3].node);

        printf("    完成语义动作: %s\n", "expr");
      }
      break;
    case 54: /* expr -> expr '%' expr  */
      {
        
    yyval.node = create_node(AST_BINARY_EXPR, NULL);
    yyval.node->op = strdup("%");
    add_child(yyval.node, yyvsp[1].node);
    add_child(yyval.node, yyvsp[3].node);

        printf("    完成语义动作: %s\n", "expr");
      }
      break;
    case 55: /* expr -> '!' expr  */
      {
        
    yyval.node = create_node(AST_UNARY_EXPR, NULL);
    yyval.node->op = strdup("!");
    add_child(yyval.node, yyvsp[2].node);

        printf("    完成语义动作: %s\n", "expr");
      }
      break;
    case 56: /* expr -> '-' expr  */
      {
        
    yyval.node = create_node(AST_UNARY_EXPR, NULL);
    yyval.node->op = strdup("-");
    add_child(yyval.node, yyvsp[2].node);

        printf("    完成语义动作: %s\n", "expr");
      }
      break;
    case 57: /* expr -> '+' expr  */
      {
        
    yyval.node = create_node(AST_UNARY_EXPR, NULL);
    yyval.node->op = strdup("+");
    add_child(yyval.node, yyvsp[2].node);

        printf("    完成语义动作: %s\n", "expr");
      }
      break;
    case 58: /* expr -> '$' expr  */
      {
        
    yyval.node = create_node(AST_UNARY_EXPR, "memory_deref");
    yyval.node->op = strdup("$");
    add_child(yyval.node, yyvsp[2].node);

        printf("    完成语义动作: %s\n", "expr");
      }
      break;
    case 59: /* expr -> '(' expr ')'  */
      {
         yyval.node = yyvsp[2].node; 
        printf("    完成语义动作: %s\n", "expr");
      }
      break;
    case 60: /* expr -> IDENT  */
      {
         yyval.node = create_node(AST_IDENT, yyvsp[1].str_val); 
        printf("    完成语义动作: %s\n", "expr");
      }
      break;
    case 61: /* expr -> IDENT '[' expr ']'  */
      {
        
    yyval.node = create_node(AST_ARRAY_ACCESS, NULL);
    add_child(yyval.node, create_node(AST_IDENT, yyvsp[1].str_val));
    add_child(yyval.node, yyvsp[3].node);

        printf("    完成语义动作: %s\n", "expr");
      }
      break;
    case 62: /* expr -> IDENT '(' args ')'  */
      {
        
    yyval.node = create_node(AST_FUNC_CALL, yyvsp[1].str_val);
    add_child(yyval.node, yyvsp[3].node);

        printf("    完成语义动作: %s\n", "expr");
      }
      break;
    case 63: /* expr -> int_literal  */
      {
         yyval.node = yyvsp[1].node; 
        printf("    完成语义动作: %s\n", "expr");
      }
      break;
    case 64: /* expr -> expr '&' expr  */
      {
        
    yyval.node = create_node(AST_BINARY_EXPR, NULL);
    yyval.node->op = strdup("&");
    add_child(yyval.node, yyvsp[1].node);
    add_child(yyval.node, yyvsp[3].node);

        printf("    完成语义动作: %s\n", "expr");
      }
      break;
    case 65: /* expr -> expr '^' expr  */
      {
        
    yyval.node = create_node(AST_BINARY_EXPR, NULL);
    yyval.node->op = strdup("^");
    add_child(yyval.node, yyvsp[1].node);
    add_child(yyval.node, yyvsp[3].node);

        printf("    完成语义动作: %s\n", "expr");
      }
      break;
    case 66: /* expr -> '~' expr  */
      {
        
    yyval.node = create_node(AST_UNARY_EXPR, NULL);
    yyval.node->op = strdup("~");
    add_child(yyval.node, yyvsp[2].node);

        printf("    完成语义动作: %s\n", "expr");
      }
      break;
    case 67: /* expr -> expr LSHIFT expr  */
      {
        
    yyval.node = create_node(AST_BINARY_EXPR, NULL);
    yyval.node->op = strdup("<<");
    add_child(yyval.node, yyvsp[1].node);
    add_child(yyval.node, yyvsp[3].node);

        printf("    完成语义动作: %s\n", "expr");
      }
      break;
    case 68: /* expr -> expr RSHIFT expr  */
      {
        
    yyval.node = create_node(AST_BINARY_EXPR, NULL);
    yyval.node->op = strdup(">>");
    add_child(yyval.node, yyvsp[1].node);
    add_child(yyval.node, yyvsp[3].node);

        printf("    完成语义动作: %s\n", "expr");
      }
      break;
    case 69: /* expr -> expr '|' expr  */
      {
        
    yyval.node = create_node(AST_BINARY_EXPR, NULL);
    yyval.node->op = strdup("|");
    add_child(yyval.node, yyvsp[1].node);
    add_child(yyval.node, yyvsp[3].node);

        printf("    完成语义动作: %s\n", "expr");
      }
      break;
    case 70: /* int_literal -> DECNUM  */
      {
        
    yyval.node = create_node(AST_INT_LITERAL, yyvsp[1].str_val);

        printf("    完成语义动作: %s\n", "int_literal");
      }
      break;
    case 71: /* int_literal -> HEXNUM  */
      {
        
    char buffer[32];
    unsigned long val = strtoul(yyvsp[1].str_val, NULL, 16);
    snprintf(buffer, sizeof(buffer), "%lu", val);
    yyval.node = create_node(AST_INT_LITERAL, buffer);

        printf("    完成语义动作: %s\n", "int_literal");
      }
      break;
    case 72: /* arg_list -> arg_list ',' expr  */
      {
        
    yyval.node = yyvsp[1].node;
    add_child(yyval.node, yyvsp[3].node);

        printf("    完成语义动作: %s\n", "arg_list");
      }
      break;
    case 73: /* arg_list -> expr  */
      {
        
    yyval.node = create_node(AST_ARG_LIST, NULL);
    add_child(yyval.node, yyvsp[1].node);

        printf("    完成语义动作: %s\n", "arg_list");
      }
      break;
    case 74: /* args -> arg_list  */
      {
         yyval.node = yyvsp[1].node; 
        printf("    完成语义动作: %s\n", "args");
      }
      break;
    case 75: /* args -> ε */
      {
         yyval.node = create_node(AST_ARG_LIST, NULL); 
        printf("    完成语义动作: %s\n", "args");
      }
      break;
    case 76: /* continue_stmt -> CONTINUE ';'  */
      {
         yyval.node = create_node(AST_CONTINUE_STMT, NULL); 
        printf("    完成语义动作: %s\n", "continue_stmt");
      }
      break;
    case 77: /* break_stmt -> BREAK ';'  */
      {
         yyval.node = create_node(AST_BREAK_STMT, NULL); 
        printf("    完成语义动作: %s\n", "break_stmt");
      }
      break;
  }

  /* 保存归约结果，主函数负责调整栈 */
  stack[*top - symbols_to_pop + 1] = yyval;
}

/* 语法分析主函数 */
int yyparse(void) {
  int state = 0;
  int top = 0;
  int token_raw;
  int token;
  int action;
  YYSTYPE stack[YYMAXDEPTH];
  int state_stack[YYMAXDEPTH];

  printf("====== 开始语法分析 ======\n");
  state_stack[0] = 0;
  token_raw = yylex();
  token = yytranslate_token(token_raw);
  printf("获取首个token: raw=%d, translated=%d\n", token_raw, token);

  while (1) {
    printf("当前状态: %d, token(raw)=%d, token(translated)=%d\n", state, token_raw, token);
    if (token == YYUNDEF) {
      printf("检测到未定义的token: %d\n", token_raw);
      yyerror("无法识别的终结符");
      return 1;
    }

    action = yytable[state * YYNTOKENS + token];

    printf("查找动作: yytable[%d * %d + %d] = %d (raw token %d)\n", state, YYNTOKENS, token, action, token_raw);

    if (action == -32767) { /* 错误 */
      /* 收集期待的 token */
      const char* expected[YYNTOKENS];
      int expected_count = 0;
      for (int i = 0; i < YYNTOKENS; i++) {
        int test_action = yytable[state * YYNTOKENS + i];
        if (test_action != -32767) {
          expected[expected_count++] = yytname[i];
        }
      }
      
      /* 记录错误 */
      extern int yylineno;
      extern char yytext[];
      add_error(yylineno, "syntax error, unexpected token", yytext, expected, expected_count);
      
      /* 输出错误并退出 */
      print_errors_json();
      return 1;
    }

    if (action > 0) { /* 移入 */
      printf("执行移入操作: 状态%d -> 状态%d\n", state, action);
      stack[++top] = yylval;
      state_stack[top] = action;
      state = action;
      int next_raw = yylex();
      int next_token = yytranslate_token(next_raw);
      printf("获取下一个token: raw=%d\n", next_raw);
      printf("转换token结果: %d -> %d\n", next_raw, next_token);
      token_raw = next_raw;
      token = next_token;
    } else if (action < 0) { /* 规约 */
      int rule = -action - 1;
      printf("执行规约操作: 使用规则%d\n", rule);
      yy_reduce(rule, &top, stack, state_stack);
      printf("规约后的栈顶位置: %d\n", top);
      /* 弹出状态栈中的规约符号对应的状态 */
      int symbols_to_pop = yyr2[rule];
      top -= symbols_to_pop;
      printf("规约后的状态栈顶: %d, 当前状态: %d\n", top, state_stack[top]);
      /* 通过GOTO表确定新状态 */
      int nonterminal = yyr1[rule] - YYNTOKENS;
      int goto_index = state_stack[top] * YYNNTS + nonterminal;
      printf("GOTO表查询: 状态%d + 非终结符%d, 索引=%d\n", state_stack[top], nonterminal, goto_index);
      if (goto_index < 0 || goto_index >= 11075) {
        printf("错误: GOTO表索引越界! goto_index=%d\n", goto_index);
        yyerror("GOTO表索引错误");
        return 3;
      }
      int next_state = yygoto[goto_index];
      printf("GOTO表结果: [%d][%d] = %d\n", state_stack[top], nonterminal, next_state);
      if (next_state == -1) {
        printf("错误: GOTO表中没有对应项! 状态%d, 非终结符%d\n", state_stack[top], nonterminal);
        yyerror("GOTO表错误");
        return 2;
      }
      /* 将新状态压入栈 */
      state_stack[++top] = next_state;
      state = next_state;
      printf("规约后的新状态: %d\n", state);
    } else { /* 接受 */
      printf("接受输入, 分析成功完成!\n");
      if (error_count > 0) {
        print_errors_json();
        return 1;
      }
      return 0;
    }
    printf("--------------------\n");
  }
  printf("====== 语法分析结束 ======\n");
  
  /* 如果有错误，输出 JSON */
  if (error_count > 0) {
    print_errors_json();
    return 1;
  }
  return 0;
}

/* 用户代码 */

int main() {
    printf("开始解析 MiniC 程序...\n");

    int result = yyparse();
    if (result == 0) {
        printf("解析成功！\n");
        printf("AST 节点总数: %d\n\n", node_count);

        if (ast_root) {
            generate_assembly(ast_root);
            free_ast(ast_root);
        }
    } else {
        printf("解析失败！\n");
    }
    
    free_globals();
    return result;
}

