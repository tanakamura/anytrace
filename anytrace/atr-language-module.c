#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <dlfcn.h>

#include "npr/symbol.h"
#include "anytrace/atr-language-module.h"

struct npr_symbol *
ATR_intern(const char *sym)
{
    return npr_intern(sym);
}

void
ATR_register_language(struct ATR *atr,
                      const struct ATR_language_module *mod)
{
    if (atr->num_language >= atr->cap_language) {
        atr->cap_language *= 2;
        size_t ns = sizeof(struct ATR_language_module) * atr->cap_language;
        atr->languages = realloc(atr->languages, ns);
    }

    memcpy(&atr->languages[atr->num_language],
           mod,
           sizeof(*mod));

    atr->languages[atr->num_language].lang_name = strdup(mod->lang_name);

    atr->num_language++;
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