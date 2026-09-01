#include "hk_schema.h"

#include <stddef.h>

hk_schema_found_t hk_schema_classify(bool present, uint32_t found, uint16_t current)
{
    if (!present) {
        return HK_SCHEMA_FOUND_ABSENT;
    }
    if (found == 0u) {
        /* Zero is never written as a version. Reading it back means the store
         * held something other than a version here. */
        return HK_SCHEMA_FOUND_CORRUPT;
    }
    if (found > UINT16_MAX) {
        return HK_SCHEMA_FOUND_CORRUPT;
    }
    if (found == current) {
        return HK_SCHEMA_FOUND_MATCH;
    }
    return (found < current) ? HK_SCHEMA_FOUND_OLDER : HK_SCHEMA_FOUND_NEWER;
}

hk_schema_action_t hk_schema_resolve(hk_store_t store, hk_schema_found_t found)
{
    if (store == HK_STORE_FACTORY) {
        switch (found) {
        case HK_SCHEMA_FOUND_MATCH:
            return HK_SCHEMA_USE;
        case HK_SCHEMA_FOUND_OLDER:
            return HK_SCHEMA_MIGRATE;
        case HK_SCHEMA_FOUND_NEWER:
            /* Written by firmware that knows more than this build does.
             * Reading what we understand is fine; writing would destroy a
             * profile we cannot represent. */
            return HK_SCHEMA_READ_ONLY;
        case HK_SCHEMA_FOUND_ABSENT:
        case HK_SCHEMA_FOUND_CORRUPT:
        default:
            /* Never write defaults here. An uncalibrated or unreadable profile
             * is a device that must not drive its drivers at normal levels,
             * and a default profile nobody measured would look like a working
             * one. Erasing it would also throw away data a human might still
             * recover. */
            return HK_SCHEMA_FAIL_SAFE;
        }
    }

    switch (found) {
    case HK_SCHEMA_FOUND_MATCH:
        return HK_SCHEMA_USE;
    case HK_SCHEMA_FOUND_OLDER:
        return HK_SCHEMA_MIGRATE;
    case HK_SCHEMA_FOUND_ABSENT:
    case HK_SCHEMA_FOUND_NEWER:
    case HK_SCHEMA_FOUND_CORRUPT:
    default:
        /* User settings are recoverable in a minute, so the priority is a
         * speaker that boots and works. This is also the rollback path: newer
         * firmware wrote settings this build cannot read, and starting from
         * defaults beats refusing to start. */
        return HK_SCHEMA_WRITE_DEFAULTS;
    }
}

/**
 * Conversions this build knows how to perform.
 *
 * Deliberately empty. There is one schema version, so there is nothing to
 * convert from; an entry here is a promise that a converter exists, and an
 * empty table is an honest statement that none does. Adding a version means
 * adding a row AND the code that performs it — the row alone would restore
 * exactly the silent failure this table was added to prevent.
 */
typedef struct {
    hk_store_t store;
    uint32_t   from_version;
    uint32_t   to_version;
} hk_schema_migration_t;

static const hk_schema_migration_t k_migrations[] = {
    /* { HK_STORE_USER, 1u, 2u },  <- with the converter, not before it */
    {HK_STORE_USER, 0u, 0u},  /* placeholder so the array is never zero-length */
};

static bool can_migrate_to(hk_store_t store, uint32_t from_version, uint32_t current)
{
    for (size_t i = 0; i < sizeof(k_migrations) / sizeof(k_migrations[0]); i++) {
        const hk_schema_migration_t *m = &k_migrations[i];
        if (m->from_version == 0u && m->to_version == 0u) {
            continue;   /* the placeholder */
        }
        if (m->store == store && m->from_version == from_version &&
            m->to_version == current) {
            return true;
        }
    }
    return false;
}

bool hk_schema_can_migrate(hk_store_t store, uint32_t from_version)
{
    const uint32_t current = (store == HK_STORE_FACTORY) ? HK_SCHEMA_FACTORY_VERSION
                                                         : HK_SCHEMA_USER_VERSION;
    return can_migrate_to(store, from_version, current);
}

hk_schema_action_t hk_schema_without_migration(hk_store_t store)
{
    /* Losing a volume setting costs the owner a minute. Misreading a driver
     * protection profile costs a tweeter. */
    return (store == HK_STORE_FACTORY) ? HK_SCHEMA_FAIL_SAFE
                                       : HK_SCHEMA_WRITE_DEFAULTS;
}

hk_schema_action_t hk_schema_plan_with(hk_store_t store, bool present,
                                       uint32_t found_version, uint16_t current)
{
    const hk_schema_found_t found = hk_schema_classify(present, found_version, current);
    const hk_schema_action_t action = hk_schema_resolve(store, found);

    if (action == HK_SCHEMA_MIGRATE &&
        !can_migrate_to(store, found_version, current)) {
        return hk_schema_without_migration(store);
    }
    return action;
}

hk_schema_action_t hk_schema_plan(hk_store_t store, bool present, uint32_t found_version)
{
    const uint16_t current = (store == HK_STORE_FACTORY)
                             ? (uint16_t)HK_SCHEMA_FACTORY_VERSION
                             : (uint16_t)HK_SCHEMA_USER_VERSION;
    return hk_schema_plan_with(store, present, found_version, current);
}

bool hk_schema_audio_permitted(hk_schema_action_t factory_action)
{
    /* Only a calibration profile this firmware fully understands permits normal
     * output. READ_ONLY still counts: the values read are valid, they simply
     * must not be written back. */
    return factory_action == HK_SCHEMA_USE ||
           factory_action == HK_SCHEMA_MIGRATE ||
           factory_action == HK_SCHEMA_READ_ONLY;
}

const char *hk_schema_action_name(hk_schema_action_t action)
{
    switch (action) {
    case HK_SCHEMA_USE:            return "use";
    case HK_SCHEMA_MIGRATE:        return "migrate";
    case HK_SCHEMA_WRITE_DEFAULTS: return "write_defaults";
    case HK_SCHEMA_READ_ONLY:      return "read_only";
    case HK_SCHEMA_FAIL_SAFE:      return "fail_safe";
    }
    return "unknown";
}

const char *hk_schema_found_name(hk_schema_found_t found)
{
    switch (found) {
    case HK_SCHEMA_FOUND_ABSENT:  return "absent";
    case HK_SCHEMA_FOUND_MATCH:   return "match";
    case HK_SCHEMA_FOUND_OLDER:   return "older";
    case HK_SCHEMA_FOUND_NEWER:   return "newer";
    case HK_SCHEMA_FOUND_CORRUPT: return "corrupt";
    }
    return "unknown";
}
