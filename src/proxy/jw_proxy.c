#include "jw_internal.h"

#include <stdlib.h>

jw_proxy_t *jw_proxy_alloc(const jw_proxy_ops_t *ops, int width, int height, void *impl)
{
    jw_proxy_t *proxy;

    proxy = (jw_proxy_t *)calloc(1, sizeof(*proxy));
    if (!proxy) {
        return NULL;
    }

    proxy->ops = ops;
    proxy->width = width;
    proxy->height = height;
    proxy->impl = impl;
    return proxy;
}

void jw_proxy_destroy(jw_proxy_t *proxy)
{
    if (!proxy) {
        return;
    }

    free(proxy->impl);
    free(proxy);
}
