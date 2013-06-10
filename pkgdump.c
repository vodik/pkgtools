#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <err.h>

#include "alpm/alpm-metadata.h"

static inline void print_list(const alpm_list_t *lst)
{
    for (; lst; lst = lst->next)
        printf("%s\n", lst->data);
}

static void dump_pkg_metadata(const alpm_pkg_meta_t *pkg, const char *var)
{
    if (strcmp(var, "pkgname") == 0)
        printf("%s\n", pkg->name);
    else if (strcmp(var, "pkgbase") == 0)
        printf("%s\n", pkg->base);
    else if (strcmp(var, "pkgver") == 0)
        printf("%s\n", pkg->version);
    else if (strcmp(var, "pkgdesc") == 0)
        printf("%s\n", pkg->desc);
    else if (strcmp(var, "url") == 0)
        printf("%s\n", pkg->url);
    else if (strcmp(var, "builddate") == 0)
        printf("%ld\n", pkg->builddate);
    else if (strcmp(var, "packager") == 0)
        printf("%s\n", pkg->packager);
    else if (strcmp(var, "size") == 0)
        printf("%ld\n", pkg->isize);
    else if (strcmp(var, "arch") == 0)
        printf("%s\n", pkg->arch);
    else if (strcmp(var, "groups") == 0)
        print_list(pkg->groups);
    else if (strcmp(var, "license") == 0)
        print_list(pkg->license);
    else if (strcmp(var, "replaces") == 0)
        print_list(pkg->replaces);
    else if (strcmp(var, "depends") == 0)
        print_list(pkg->depends);
    else if (strcmp(var, "conflicts") == 0)
        print_list(pkg->conflicts);
    else if (strcmp(var, "provides") == 0)
        print_list(pkg->provides);
    else if (strcmp(var, "optdepends") == 0)
        print_list(pkg->optdepends);
    else if (strcmp(var, "makedepends") == 0)
        print_list(pkg->makedepends);
    else if (strcmp(var, "checkdepends") == 0)
        print_list(pkg->checkdepends);
}

int main(int argc, char *argv[])
{
    int i;
    alpm_pkg_meta_t *pkg = NULL;

    if (argc < 3)
        errx(EXIT_FAILURE, "not enough arguments");

    alpm_pkg_load_metadata(argv[argc - 1], &pkg);

    for (i = 1; i < argc - 1; ++i)
        dump_pkg_metadata(pkg, argv[i]);
}
