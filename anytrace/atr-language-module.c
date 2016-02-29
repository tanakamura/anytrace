#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <dlfcn.h>

#include "npr/symbol.h"
#include "npr/varray.h"

#include "anytrace/atr-language-module.h"
#include "anytrace/atr-impl.h"

struct npr_symbol *
ATR_intern(const char *sym)
{
    return npr_intern(sym);
}

struct int_pair {
    int mod, sym;
};

void
ATR_register_language(struct ATR *atr,
                      const struct ATR_language_module *mod)
{
    if (atr->num_language >= atr->impl->cap_language) {
        atr->impl->cap_language *= 2;
        size_t ns = sizeof(struct ATR_language_module) * atr->impl->cap_language;
        atr->languages = realloc(atr->languages, ns);
    }

    int pos = atr->num_language;
    memcpy(&atr->languages[pos],
           mod,
           sizeof(*mod));

    atr->languages[pos].lang_name = strdup(mod->lang_name);

    atr->num_language++;

    int num_symbol = mod->num_symbol;
    for (int li=0; li<num_symbol; li++) {
        struct npr_symtab_entry *e = npr_symtab_lookup_entry(&atr->impl->lang_module_hook_table,
                                                             mod->hook_func_name_list[li],
                                                             NPR_LOOKUP_APPEND);
        struct int_pair *p = malloc(sizeof(*p));
        p->mod = pos;
        p->sym = li;

        e->data = p;
    }

    atr->languages[pos].hook_arg_list = malloc(sizeof(void*) * num_symbol);
    memcpy(atr->languages[pos].hook_arg_list,
           mod->hook_arg_list,
           sizeof(void*) * num_symbol);
}

static void
load_lang_module_dir(struct ATR* atr, const char *path)
{
    DIR *files = opendir(path);
    if (files == NULL) {
        return;
    }

    while (1) {
        struct dirent entry, *result;
        int r = readdir_r(files, &entry, &result);

        if (r != 0 || result == NULL) {
            break;
        }

        size_t relpath_len = strlen(path) + 1 + strlen(result->d_name) + 1;
        char *relpath = malloc(relpath_len);
        sprintf(relpath, "%s/%s", path, result->d_name);

        void *h = dlopen(relpath, RTLD_LAZY);
        free(relpath);

        if (h) {
            void (*i)(struct ATR*) = (void (*)(struct ATR*))dlsym(h, "ATR_language_init");
            if (i) {
                i(atr);
            } else {
                dlclose(h);
            }
        }
    }

    closedir(files);
}

struct ATR_frame_builder {
    struct npr_varray *frames;
};

void
ATR_load_language_module(struct ATR *atr)
{
    load_lang_module_dir(atr, INSTALL_PREFIX "/lib/anytrace");
    load_lang_module_dir(atr, ".");

    char *path = getenv("ANYTRACE_MODULE_PATH");
    if (path) {
        load_lang_module_dir(atr, path);
    }
}

int
ATR_run_language_hook(struct ATR *atr,
                      struct ATR_backtracer *tr,
                      struct npr_varray *machine_frame)
{
    struct ATR_stack_frame_entry *machine_top;
    machine_top = VA_LAST_PTR(struct ATR_stack_frame_entry, machine_frame);

    if (! (machine_top->flags & ATR_FRAME_HAVE_SYMBOL)) {
        return 0;
    }

    struct npr_symbol *func = machine_top->symbol;

    struct npr_symtab_entry *entry;
    entry = npr_symtab_lookup_entry(&atr->impl->lang_module_hook_table,
                                    func,
                                    NPR_LOOKUP_FAIL);
    if (entry == NULL) {
        return 0;
    }

    struct int_pair *idx = (struct int_pair*)entry->data;
    struct ATR_language_module *lang = &atr->languages[idx->mod];
    void *hook_arg = lang->hook_arg_list[idx->sym];

    struct ATR_frame_builder fb;
    int r = -1;
    struct npr_varray lang_stack;

    if (lang->flags & ATR_LANGUAGE_USE_OWN_STACK) {
        fb.frames = &lang_stack;
        npr_varray_init(&lang_stack, sizeof(struct ATR_stack_frame_entry), 4);
    } else {
        fb.frames = machine_frame;
    }

    r = lang->symbol_hook(atr, tr, &fb, hook_arg);
    if (r < 0) {
        if (lang->flags & ATR_LANGUAGE_USE_OWN_STACK) {
            npr_varray_discard(fb.frames);
        }

        return 0;               /* ?? */
    }

    if (lang->flags & ATR_LANGUAGE_USE_OWN_STACK) {
        machine_top->num_child_frame = lang_stack.nelem;
        machine_top->child_frame = npr_varray_malloc_close(&lang_stack);
    }
    return 0;
}