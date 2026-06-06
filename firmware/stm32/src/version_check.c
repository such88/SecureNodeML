#include "version_check.h"
#include <zephyr/logging/log.h>
#include <zephyr/kernel.h>
#include <errno.h>

LOG_MODULE_REGISTER(version_check, LOG_LEVEL_INF);

static uint32_t g_current = 0U;

int version_check_and_update(uint32_t incoming)
{
    LOG_INF("Version check: incoming=%u  stored=%u",
            incoming, g_current);

    if (incoming <= g_current) {
        LOG_ERR("ROLLBACK REJECTED: v%u <= stored v%u",
                incoming, g_current);
        return -EACCES;
    }

    g_current = incoming;
    LOG_INF("Version accepted: now v%u", g_current);
    return 0;
}

uint32_t version_get_current(void)
{
    return g_current;
}
