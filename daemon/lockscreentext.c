
#include <errno.h>
#include <sqlite3.h>
#include <stdlib.h>

#include "../common.h"
#include "../requestid.h"
#include "util.h"

// constants for name on lock screen feature
// full path to the sqlite db needed
#define LOCKTEXT_SQLITE_PATH "/data/system/locksettings.db"

static int replace_locksetting(sqlite3 *db, const char *name, int user,
                               const char *value) {
  sqlite3_stmt *stmt = NULL;
  int rc;

  rc = sqlite3_prepare_v2(
      db, "DELETE FROM locksettings WHERE name = ? AND user = ?", -1, &stmt,
      NULL);
  if (rc != SQLITE_OK)
    return rc;

  sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC);
  sqlite3_bind_int(stmt, 2, user);

  rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);

  if (rc != SQLITE_DONE)
    return rc;

  rc = sqlite3_prepare_v2(
      db, "INSERT INTO locksettings(name, user, value) VALUES (?, ?, ?)", -1,
      &stmt, NULL);
  if (rc != SQLITE_OK)
    return rc;

  sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC);
  sqlite3_bind_int(stmt, 2, user);

  /*
   * TRANSIENT makes SQLite copy the caller's string, which is
   * appropriate for arbitrary/untrusted input.
   */
  sqlite3_bind_text(stmt, 3, value, -1, SQLITE_TRANSIENT);

  rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);

  return rc == SQLITE_DONE ? SQLITE_OK : rc;
}

static int set_lock_screen_owner_info(const char *text) {
  sqlite3 *db = NULL;
  int rc;

  rc = sqlite3_open_v2(LOCKTEXT_SQLITE_PATH, &db, SQLITE_OPEN_READWRITE, NULL);
  if (rc != SQLITE_OK)
    goto out;

  rc = sqlite3_exec(db, "BEGIN IMMEDIATE", NULL, NULL, NULL);
  if (rc != SQLITE_OK)
    goto out;

  rc = replace_locksetting(db, "lock_screen_owner_info", 0, text);
  if (rc != SQLITE_OK)
    goto rollback;

  rc = replace_locksetting(db, "lock_screen_owner_info_enabled", 0, "1");
  if (rc != SQLITE_OK)
    goto rollback;

  rc = sqlite3_exec(db, "COMMIT", NULL, NULL, NULL);
  goto out;

rollback:
  sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);

out:
  if (rc != SQLITE_OK)
    LOG_ERR("sqlite error: %s\n", db ? sqlite3_errmsg(db) : "unknown");

  sqlite3_close(db);
  return rc;
}

int maybe_set_lockscreen_text(void) {
  sqlite3 *db = NULL;
  sqlite3_stmt *stmt = NULL;
  int err;
  struct DUMBOS_USER_DATA *user_data;
  char *fancy_name;

  LOG_DEBUG("maybe_set_lockscreen_text()");

  // getting the fancy name
  if ((user_data = dumbos_alloc_get_user()) == NULL) {
    LOG_ERROR("maybe_set_lockscreen_text(): no user available");
    errno = EOWNERDEAD;
    return -EOWNERDEAD;
  }
  if (dumbos_get_fancy_name(user_data)) {
    fancy_name = strdup(dumbos_get_fancy_name(user_data));
  } else {
    fancy_name = NULL;
  }
  free(user_data);
  if (fancy_name == NULL) {
    LOG_ERRNO("failed to get fancy_name", errno);
    return -1;
  }

  if (set_lock_screen_owner_info(fancy_name) != SQLITE_OK) {
    LOG_ERR(
        "maybe_set_lockscreen_text(): failed to update " LOCKTEXT_SQLITE_PATH);
    free(fancy_name);
    errno = EIO;
    return -1;
  }

  free(fancy_name);
  return 0;
}
