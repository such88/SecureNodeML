/*
 * src/version_check.c — Anti-rollback model version enforcement
 *
 * Stores the highest accepted model version in NVS (non-volatile storage).
 * Any OTA model with version <= stored version is rejected.
 *
 * Requires in prj.conf: CONFIG_SETTINGS=y, CONFIG_SETTINGS_NVS=y
 * Requires flash partition "storage" in board overlay (sector 11)
 *
 * TODO Day 17: call version_check_and_update() from main.c after verify
 */
#include "version_check.h"
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>
#include <errno.h>
#include <string.h>

LOG_MODULE_REGISTER(version_check, LOG_LEVEL_INF);

#define VER_KEY "model/ver"

static uint32_t g_current = 0U;
static bool     g_loaded  = false;

static int ver_set_cb(const char *key, size_t len,
                      settings_read_cb read_cb, void *cb_arg)
{
    if ((strcmp(key, "ver") == 0) && (len == sizeof(uint32_t))) {
        read_cb(cb_arg, &g_current, sizeof(g_current));
        LOG_DBG("Loaded stored version: %u", g_current);
    }
    return 0;
}

static struct settings_handler ver_handler = {
    .name  = "model",
    .h_set = ver_set_cb,
};

static void lazy_load(void)
{
    if (!g_loaded) {
        settings_subsys_init();
        settings_register(&ver_handler);
        settings_load();
        g_loaded = true;
    }
}

int version_check_and_update(uint32_t incoming)
{
    lazy_load();

    LOG_INF("Version check: incoming=%u  stored=%u", incoming, g_current);

    if (incoming <= g_current) {
        LOG_ERR("ROLLBACK REJECTED: v%u <= stored v%u", incoming, g_current);
        return -EACCES;
    }

    g_current = incoming;
    int ret = settings_save_one(VER_KEY, &g_current, sizeof(g_current));
    if (ret != 0) {
        LOG_ERR("NVS save failed: %d", ret);
        return ret;
    }

    LOG_INF("Version accepted: now v%u (persisted)", g_current);
    return 0;
}

uint32_t version_get_current(void)
{
    lazy_load();
    return g_current;
}
