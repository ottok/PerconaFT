/* -*- mode: C; c-basic-offset: 4 -*- */
#ifndef TOKU_CHECKPOINT_H
#define TOKU_CHECKPOINT_H

#ident "Copyright (c) 2009-2010 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."
#ident "$Id$"

#if defined(__cplusplus) || defined(__cilkplusplus)
extern "C" {
#endif

int toku_set_checkpoint_period(CACHETABLE ct, u_int32_t new_period);
//Effect: Change [end checkpoint (n) - begin checkpoint (n+1)] delay to
//        new_period seconds.  0 means disable.

u_int32_t toku_get_checkpoint_period(CACHETABLE ct);
u_int32_t toku_get_checkpoint_period_unlocked(CACHETABLE ct);


/******
 *
 * NOTE: checkpoint_safe_lock is highest level lock
 *       multi_operation_lock is next level lock
 *       ydb_big_lock is next level lock
 *
 *       Locks must always be taken in this sequence (highest level first).
 *
 */


/****** 
 * Client code must hold the checkpoint_safe lock during the following operations:
 *       - delete a dictionary via DB->remove
 *       - delete a dictionary via DB_TXN->abort(txn) (where txn created a dictionary)
 *       - rename a dictionary //TODO: Handlerton rename needs to take this
 *                             //TODO: Handlerton rename needs to be recoded for transaction recovery
 *****/

void toku_checkpoint_safe_client_lock(void);

void toku_checkpoint_safe_client_unlock(void);



/****** 
 * These functions are called from the ydb level.
 * Client code must hold the multi_operation lock during the following operations:
 *       - insertion into multiple indexes
 *       - replace into (simultaneous delete/insert on a single key)
 *****/

void toku_multi_operation_client_lock(void);

void toku_multi_operation_client_unlock(void);


// Initialize the checkpoint mechanism, must be called before any client operations.
// Must pass in function pointers to take/release ydb lock.
int toku_checkpoint_init(void (*ydb_lock_callback)(void), void (*ydb_unlock_callback)(void));

int toku_checkpoint_destroy(void);

typedef enum {SCHEDULED_CHECKPOINT  = 0,   // "normal" checkpoint taken on checkpoint thread
	      CLIENT_CHECKPOINT     = 1,   // induced by client, such as FLUSH LOGS or SAVEPOINT
	      TXN_COMMIT_CHECKPOINT = 2,   
	      STARTUP_CHECKPOINT    = 3, 
	      UPGRADE_CHECKPOINT    = 4,
	      RECOVERY_CHECKPOINT   = 5,
	      SHUTDOWN_CHECKPOINT   = 6} checkpoint_caller_t;

// Take a checkpoint of all currently open dictionaries
// Callbacks are called during checkpoint procedure while checkpoint_safe lock is still held.
// Callbacks are primarily intended for use in testing.
// caller_id identifies why the checkpoint is being taken.
int toku_checkpoint(CACHETABLE ct, TOKULOGGER logger,
		    void (*callback_f)(void*),  void * extra,
		    void (*callback2_f)(void*), void * extra2,
		    checkpoint_caller_t caller_id);



/****** 
 * These functions are called from the ydb level.
 * They return status information and have no side effects.
 * Some status information may be incorrect because no locks are taken to collect status.
 * (If checkpoint is in progress, it may overwrite status info while it is being read.)
 *****/
typedef struct {
    uint64_t footprint;
    time_t time_last_checkpoint_begin_complete;
    time_t time_last_checkpoint_begin;
    time_t time_last_checkpoint_end;
    uint64_t last_lsn;
    uint64_t checkpoint_count;
    uint64_t checkpoint_count_fail;
    uint64_t waiters_now;    // how many threads are currently waiting for the checkpoint_safe lock to perform a checkpoint
    uint64_t waiters_max;    // max threads ever simultaneously waiting for the checkpoint_safe lock to perform a checkpoint
    uint64_t client_wait_on_mo;   // how many times a client thread waited for the multi_operation lock
    uint64_t client_wait_on_cs;   // how many times a client thread waited for the checkpoint_safe lock
    uint64_t cp_wait_sched;       // how many times a scheduled checkpoint waited for the checkpoint_safe lock
    uint64_t cp_wait_client;      // how many times a client checkpoint waited for the checkpoint_safe lock
    uint64_t cp_wait_txn;         // how many times a txn_commit checkpoint waited for the checkpoint_safe lock
    uint64_t cp_wait_other;       // how many times a checkpoint for another purpose waited for the checkpoint_safe lock 
} CHECKPOINT_STATUS_S, *CHECKPOINT_STATUS;

void toku_checkpoint_get_status(CHECKPOINT_STATUS stat);

#if defined(__cplusplus) || defined(__cilkplusplus)
};
#endif

#endif
