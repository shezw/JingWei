#include "jw_internal.h"

#include <stdlib.h>

jw_event_manager_t *jw_event_manager_create_mouse(jw_proxy_t *proxy)
{
    jw_event_manager_t *manager;

    if (!proxy) {
        return NULL;
    }

    manager = (jw_event_manager_t *)calloc(1, sizeof(*manager));
    if (!manager) {
        return NULL;
    }

    manager->proxy = proxy;
    return manager;
}

void jw_event_manager_destroy(jw_event_manager_t *manager)
{
    free(manager);
}

int jw_event_manager_poll(jw_event_manager_t *manager, jw_event_t *event)
{
    if (!manager || !manager->proxy || !event) {
        return -1;
    }

    if (!manager->proxy->ops || !manager->proxy->ops->poll_event) {
        return 0;
    }

    return manager->proxy->ops->poll_event(manager->proxy, event);
}
