/* 
 * Copyright (c) 2010, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 */
/*
 *  ======== SemMP_posix.c ========
 */

#include <xdc/std.h>

#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>

#include <ti/sdo/utils/trace/gt.h>
#include <ti/sdo/ce/osal/Memory.h>
#include <ti/sdo/ce/osal/Global.h>

#include <ti/sdo/ce/osal/SemMP.h>

#include <errno.h>

/*
 *  Linux IPC semaphores are identified by a 32 bit key, and the same semaphore
 *  can be obtained accross processes by calling semget() with the same key.
 *  These semaphores actually consist of an array of semaphores. We use a
 *  semaphore with an array of 2 semaphores to implement the SemMP_Obj. The
 *  first semaphore in the array will hold the semaphore count, and the second
 *  will hold a reference count of the number of times that the semaphore
 *  has been "created" (ie the number of times semget() has been called for
 *  the given key).
 */
#define _SemMP_SEMCOUNT 0 /* Array index of semaphore holding the count */
#define _SemMP_REFCOUNT 1 /* Array index of semaphore holding the ref count */

#define NUMSEMS 2 /* Size of semaphore array */

/*
 *  ID of semaphore for handling critical sections in this module. This is
 *  auto-generated by Global.xdt.
 */
extern UInt32 ti_sdo_ce_osal_linux_SemMP_ipcKey;

/* Maximum tries to enter critical section before printing warning */
#define MAXCOUNT 100000



/*
 *  ======== SemMP_Obj ========
 */
typedef struct SemMP_Obj {
    Int id;     /* ID of IPC semaphore */
} SemMP_Obj;

/*
 *  ======== SemUnion ========
 *  Linux semctl() calls take as one of the arguments, a parameter of the
 *  following type:
 *
 *  union semun {
 *     int              val;    * Value for SETVAL *
 *     struct semid_ds *buf;    * Buffer for IPC_STAT, IPC_SET *
 *     unsigned short  *array;  * Array for GETALL, SETALL *
 *     struct seminfo  *__buf;  * Buffer for IPC_INFO (Linux specific) *
 * };
 *
 *  In some earlier versions of glibc, the semun union was defined in
 *  <sys/sem.h>, but POSIX.1-2001 requires that the caller define this union.
 *  On versions of glibc where this union is not defined, the macro
 *  _SEM_SEMUN_UNDEFINED is defined in <sys/sem.h>.
 */
typedef union SemUnion {
    Int val;
    struct semid_ds *buf;
    unsigned short *array;
    struct seminfo  *__buf;
} SemUnion;

/*
 *  REMINDER: If you add an initialized static variable, reinitialize it at
 *  cleanup
 */
static Int curInit = 0;
static GT_Mask curTrace = {NULL, NULL};

static Void    cleanup(Void);

static Int enterCS(Int key);
static Void exitCS(Int id);

/*
 *  ======== SemMP_create ========
 */
SemMP_Handle SemMP_create(Int key, Int count)
{
    Int     mutexId;
    Int     semId;
    SemMP_Obj *sem;
    SemUnion semU;
    struct sembuf semBuf;

    GT_2trace(curTrace, GT_ENTER, "SemMP_create> key: 0x%x count: %d\n", key,
            count);

    if ((sem = (SemMP_Obj *)Memory_alloc(sizeof(SemMP_Obj), NULL)) == NULL) {
        GT_0trace(curTrace, GT_7CLASS, "SemMP_create> Memory_alloc failed\n");
        return (NULL);
    }

    /* Enter critical section */
    mutexId = enterCS(ti_sdo_ce_osal_linux_SemMP_ipcKey);

    /*
     *  First try to create semaphore with exclusive access, so we know
     *  whether or not it has already been created.
     */
    semId = semget((key_t)key, NUMSEMS, 0666 | IPC_CREAT | IPC_EXCL);

    if (semId == -1) {
        /* Semaphore may have already been created */
        semId = semget((key_t)key, NUMSEMS, 0666 | IPC_CREAT);
        if (semId == -1) {
            /* Error */

            /* Leave critical section */
            exitCS(mutexId);

            GT_1trace(curTrace, GT_7CLASS, "SemMP_create> semget 0x%x failed\n",
                    key);
            Memory_free(sem, sizeof(SemMP_Obj), NULL);
            return (NULL);
        }

        /* Successfully attached to existing semaphore */
    }
    else {
        /*
         *  This is the first time this semaphore has been created, so
         *  we need to initialize its value.
         */
        semU.val = count;
        if (semctl(semId, _SemMP_SEMCOUNT, SETVAL, semU) == -1) {
            GT_1trace(curTrace, GT_7CLASS,
                    "SemMP_create> Failed to init count of 0x%x\n", semId);

            /*
             *  Failed to set value, try to delete semaphore. Note: the second
             *  argument (semaphore number) will be ignored in this call.
             */
            if (semctl(semId, 0, IPC_RMID, semU) == -1) {
                GT_1trace(curTrace, GT_7CLASS,
                        "SemMP_create> semctl(0x%x, 0, IPC_RMID,...) failed\n",
                        semId);
            }

            /* Leave critical section */
            exitCS(mutexId);

            Memory_free(sem, sizeof(SemMP_Obj), NULL);
            return (NULL);
        }

        /* Use the second semaphore as a reference count, initialize to 0 */
        semU.val = 0;
        if (semctl(semId, _SemMP_REFCOUNT, SETVAL, semU) == -1) {
            GT_1trace(curTrace, GT_7CLASS,
                    "SemMP_create> Failed to init ref count of 0x%x\n", semId);

            /*
             *  Failed to set value, try to delete semaphore. Note: the second
             *  argument (semaphore number) will be ignored in this call.
             */
            if (semctl(semId, 0, IPC_RMID, semU) == -1) {
                GT_1trace(curTrace, GT_7CLASS,
                        "SemMP_create> semctl(0x%x, 0, IPC_RMID,...) failed\n",
                        semId);
            }

            /* Leave critical section */
            exitCS(mutexId);

            Memory_free(sem, sizeof(SemMP_Obj), NULL);
            return (NULL);
        }
    }

    sem->id = semId;

    /* Increment reference count */
    semBuf.sem_num = _SemMP_REFCOUNT;
    semBuf.sem_op = 1;
    semBuf.sem_flg = SEM_UNDO;

    if (semop(semId, &semBuf, 1) == -1) {
        GT_1trace(curTrace, GT_7CLASS,
                "SemMP_create> Failed to increment ref count on 0x%x\n", semId);
        GT_assert(curTrace, 0 == 1);
    }

    GT_2trace(curTrace, GT_ENTER, "SemMP_create> semId: 0x%x refCount: %d\n",
            semId, semctl(semId, _SemMP_REFCOUNT, GETVAL, semU));

    /* Leave critical section */
    exitCS(mutexId);

    GT_1trace(curTrace, GT_ENTER, "Leaving SemMP_create> sem[0x%x]\n", sem);

    return (sem);
}

/*
 *  ======== SemMP_delete ========
 */
Void SemMP_delete(SemMP_Handle sem)
{
    Int     mutexId;
    Int     refCount;
    SemUnion semU;
    struct sembuf semBuf;

    GT_1trace(curTrace, GT_ENTER, "Entered SemMP_delete> sem[0x%x]\n", sem);

    if (sem != NULL) {
        /* Enter critical section */
        mutexId = enterCS(ti_sdo_ce_osal_linux_SemMP_ipcKey);

        GT_2trace(curTrace, GT_ENTER, "SemMP_delete> sem: 0x%x, ref count: %d\n",
                sem->id, semctl(sem->id, _SemMP_REFCOUNT, GETVAL, semU));

        /* Decrement the reference count */
        semBuf.sem_num = _SemMP_REFCOUNT;
        semBuf.sem_op = -1;
        semBuf.sem_flg = SEM_UNDO;

        if (semop(sem->id, &semBuf, 1) == -1) {
            GT_1trace(curTrace, GT_7CLASS,
                    "Failed to decrement reference count on sem 0x%x\n",
                    sem->id);
        }

        refCount = semctl(sem->id, _SemMP_REFCOUNT, GETVAL, semU);

        if (refCount == 0) {
            /* Try to remove the semaphore */
            if (semctl(sem->id, 0, IPC_RMID, semU) == -1) {
                GT_1trace(curTrace, GT_7CLASS,
                        "SemMP_delete> Failed to delete semaphore: 0x%x\n",
                        sem->id);
            }
            else {
                GT_1trace(curTrace, GT_1CLASS,
                        "SemMP_delete> Deleted semaphore: 0x%x\n", sem->id);
            }
        }
        else if (refCount < 0) {
            GT_2trace(curTrace, GT_7CLASS,
                    "SemMP_delete> semctl returned ref count %d on sem 0x%x\n",
                    refCount, sem->id);
            GT_assert(curTrace, refCount >= 0);
        }

        /* Leave critical section */
        exitCS(mutexId);

        Memory_free(sem, sizeof(SemMP_Obj), NULL);
    }

    GT_0trace(curTrace, GT_ENTER, "Leaving SemMP_delete>\n");
}

/*
 *  ======== SemMP_getCount ========
 */
Int SemMP_getCount(SemMP_Handle sem)
{
    SemUnion semU;
    Int      count;

    count = semctl(sem->id, _SemMP_SEMCOUNT, GETVAL, semU);

    return (count);
}

/*
 *  ======== SemMP_getRefCount ========
 */
Int SemMP_getRefCount(SemMP_Handle sem)
{
    SemUnion semU;
    Int      count;

    count = semctl(sem->id, _SemMP_REFCOUNT, GETVAL, semU);

    return (count);
}

/*
 *  ======== SemMP_init ========
 */
Void SemMP_init(Void)
{
    if (curInit++ == 0) {
        GT_create(&curTrace, SemMP_GTNAME);

        Global_atexit((Fxn)cleanup);
    }
}

/*
 *  ======== SemMP_pend ========
 */
Int SemMP_pend(SemMP_Handle sem, UInt32 timeout)
{
    Int    status = SemMP_EOK;
    struct sembuf semBuf;

    GT_2trace(curTrace, GT_ENTER,
            "Entered SemMP_pend> sem[0x%x] timeout[0x%x]\n", sem, timeout);

    /* Timeouts not supported yet */
    GT_assert(curTrace, timeout == SemMP_FOREVER);

    semBuf.sem_num = _SemMP_SEMCOUNT;
    semBuf.sem_op = -1;
    semBuf.sem_flg = SEM_UNDO;

    /* TODO: Figure out how to do timeout */
	while (semop(sem->id, &semBuf, 1) == -1) {
		if (errno != EINTR) {
			status = SemMP_EFAIL;
			GT_1trace(curTrace, GT_7CLASS, "SemMP_pend [0x%x] failed\n", sem->id);
			GT_assert(curTrace, status == SemMP_EOK);
		}
	}

    GT_2trace(curTrace, GT_ENTER, "Leaving SemMP_pend> sem[0x%x] status[%d]\n",
            sem, status);

    return (status);
}

/*
 *  ======== SemMP_post ========
 */
Void SemMP_post(SemMP_Handle sem)
{
    struct sembuf semBuf;

    GT_1trace(curTrace, GT_ENTER, "Entered SemMP_post> sem[0x%x]\n", sem);

    semBuf.sem_num = _SemMP_SEMCOUNT;
    semBuf.sem_op = 1;
    semBuf.sem_flg = SEM_UNDO;

    if (semop(sem->id, &semBuf, 1) == -1) {
        /* Should never happen */
        GT_1trace(curTrace, GT_7CLASS, "SemMP_post [0x%x] failed\n", sem->id);
    }

    GT_1trace(curTrace, GT_ENTER, "Leaving SemMP_post> sem[0x%x]\n", sem);
}

/*
 *  ======== cleanup ========
 */
static Void cleanup(Void)
{
    GT_assert(curTrace, curInit > 0);

    if (--curInit == 0) {
        /* Nothing to do */
    }
}

/*
 *  ======== enterCS ========
 *  Enter critical section
 *  WARNING: Calls to this function with the same key cannot be nested!
 */
static Int enterCS(Int key)
{
    Int     count = 0;
    Int     id = 0;

    GT_1trace(curTrace, GT_ENTER, "Entered enterCS> key[0x%x]\n", key);

    /*
     *  Attempt to create a new semaphore with this key, for exclusive
     *  access. If this call fails, we assume it's because another process
     *  has created it. In that case we loop until we can create it.
     */
    id = semget((key_t)key, 1, 0666 | IPC_CREAT | IPC_EXCL);

    while (id == -1) {
        /* Yield */
        count++;

        if (count > MAXCOUNT) {
            GT_1trace(curTrace, GT_6CLASS,
                    "enterCS> Timing out trying to enter critical "
                    "section, key = 0x%x\n", key);
            count = 0;
        }

        usleep(1);

        /*
         *  Wait til the semaphore is removed by the other process so we
         *  can get it for ourself.
         */
        id = semget((key_t)key, 1, 0666 | IPC_CREAT | IPC_EXCL);
    }

    GT_1trace(curTrace, GT_ENTER, "Leaving enterCS> id[0x%x]\n", id);

    return (id);
}

/*
 *  ======== exitCS ========
 *  Exit critical section
 */
static Void exitCS(Int id)
{
    SemUnion semU;

    GT_1trace(curTrace, GT_ENTER, "Entered exitCS> id[0x%x]\n", id);

    /*
     *  Delete the semaphore  Note: the second argument (semaphore number)
     *  will be ignored in this call.
     */
    if (semctl(id, 0, IPC_RMID, semU) == -1) {
        /* Error that should never happen. */
        GT_1trace(curTrace, GT_7CLASS,
                "exitCS> semctl(%d, 0, IPC_RMID,...) failed!\n", id);
        GT_assert(curTrace, 0 == 1);
    }

    GT_0trace(curTrace, GT_ENTER, "Leaving exitCS\n");
}
/*
 *  @(#) ti.sdo.ce.osal.linux; 2, 0, 1,181; 12-2-2010 21:24:47; /db/atree/library/trees/ce/ce-r11x/src/ xlibrary

 */

