#include "hk_schema.h"

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
