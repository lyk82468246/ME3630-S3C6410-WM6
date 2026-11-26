/* Host-runnable Win32 regression test for D41 production headers/includes.
 * Compiles the exact production me3630_open.inc / me3630_mask.inc /
 * me3630_purge.inc / me3630_events.inc / me3630_queue.inc / me3630_notify_state.h
 * / me3630_timeout.h code and runs the full Unimodem WatchForDisconnect (hDevice_r0)
 * + AsyncMac (hDevice) concurrency matrix on the build host.
 */
#define COM_V8
#define COM_V9
#define COM_V15
#define COM_V17
#define COM_V18
#define COM_DIAG34
#define COM_DIAG35
#define COM_DIAG36
#define COM_DIAG37
#define COM_DIAG38
#define COM_DIAG39
#define COM_DIAG41
#include <windows.h>
#include <stdio.h>
#include "me3630_timeout.h"
#include "me3630_notify_state.h"

typedef struct { HANDLE stop; } PROBE;
struct M36_OPEN_TAG;
typedef struct {
    PROBE *parent;
    HANDLE in, out, rxReady, eventWake;
    CRITICAL_SECTION io, rxIO;
    COMMTIMEOUTS timeouts;
    DWORD index, qHead, qCount, rxFailure, eventMask, eventPending;
    DWORD rxAbortEpoch, txClearEpoch, txQueued, lastRxTick;
    BYTE queue[4096];
    DWORD eventEpoch;
    LONG refs, opened, eventWaiter;
    M36_NOTIFY_STATE notifyState;
    struct M36_OPEN_TAG *openList;
} PORT;

static BOOL serial_alive(PORT *p) { return WaitForSingleObject(p->parent->stop, 0) != WAIT_OBJECT_0; }
static void port_release(PORT *p) { InterlockedDecrement(&p->refs); }
static void logline(LPCWSTR format, ...) { (void)format; }
#define LIFE(event, object, value, error) ((void)0)
static int serial_transfer(PORT *p, HANDLE pipe, LPVOID buf, DWORD size, BOOL input, DWORD timeout, PDWORD bytes)
{ (void)p; (void)pipe; (void)buf; (void)size; (void)input; (void)timeout; *bytes = 0; return 0; }

#include "me3630_open.inc"
#include "me3630_ring.inc"
#include "me3630_queue.inc"
#include "me3630_purge.inc"
#include "me3630_mask.inc"
#include "me3630_events.inc"

static DWORD checks = 0, failures = 0;
static void check(const char *name, BOOL ok)
{
    checks++;
    if (!ok) failures++;
    printf("CHECK %-42s pass=%lu\n", name, (unsigned long)(ok ? 1 : 0));
}

typedef struct {
    M36_OPEN *open;
    HANDLE done;
    DWORD mask;
    BOOL ok;
    DWORD err;
} WAIT_JOB;

static DWORD WINAPI wait_worker(LPVOID arg)
{
    WAIT_JOB *j = (WAIT_JOB*)arg;
    DWORD actual = 0;
    j->mask = 0xdeadbeef;
    j->ok = serial_wait_event_open(j->open, (PBYTE)&j->mask, sizeof(DWORD), &actual);
    j->err = GetLastError();
    SetEvent(j->done);
    return 0;
}

int main(void)
{
    PROBE parent;
    PORT p;
    M36_OPEN *dataOpen, *r0Open, *r0Second, *dupData;
    WAIT_JOB jData, jR0, jBusy;
    HANDLE tData, tR0;
    BYTE pkt[10] = { 0xa1, 0x20, 0, 0, 1, 0, 2, 0, 0, 0 };
    DWORD fired = 0, w, begin;

    memset(&p, 0, sizeof(p));
    parent.stop = CreateEvent(NULL, TRUE, FALSE, NULL);
    p.parent = &parent;
    p.refs = 1;
    p.index = 1;
    p.rxReady = CreateEvent(NULL, TRUE, FALSE, NULL);
    p.eventWake = CreateEvent(NULL, TRUE, FALSE, NULL);
    p.notifyState.size = sizeof(M36_NOTIFY_STATE);
    InitializeCriticalSection(&p.io);
    InitializeCriticalSection(&p.rxIO);

    /* 1. Ownership & control-code permissions matching CE5 COM_MDD2 */
    dataOpen = m36_open_new(&p, GENERIC_READ | GENERIC_WRITE);
    r0Open = m36_open_new(&p, 0);
    r0Second = m36_open_new(&p, 0);
    dupData = m36_open_new(&p, GENERIC_READ);
    check("ownership_data_plus_two_r0", dataOpen && r0Open && r0Second && !dupData && GetLastError() == ERROR_SHARING_VIOLATION && p.opened == 1 && p.refs == 4);
    check("r0_allows_get_set_wait_modemstatus",
        m36_open_allows_control(r0Open, 0x001b0024) &&
        m36_open_allows_control(r0Open, 0x001b0028) &&
        m36_open_allows_control(r0Open, 0x001b002c) &&
        m36_open_allows_control(r0Open, 0x001b0034) &&
        m36_open_allows_control(r0Open, 0x001b0040) &&
        m36_open_allows_control(r0Open, 0x001b0050));
    check("r0_denies_mutating_ioctls",
        !m36_open_allows_control(r0Open, 0x001b0054) &&
        !m36_open_allows_control(r0Open, 0x001b003c) &&
        !m36_open_allows_control(r0Open, 0x001b0044) &&
        !m36_open_allows_control(r0Open, 0x001b000c));
    m36_open_release(r0Second);
    check("close_second_r0_preserves_data_and_r0", p.opened == 1 && p.refs == 3);

    /* 2. Independent masks & concurrent waiters: AsyncMac (EV_RXCHAR) + Unimodem (EV_RLSD) */
    EnterCriticalSection(&p.io);
    check("independent_masks_set",
        m36_open_set_event_mask(dataOpen, EV_RXCHAR) &&
        m36_open_set_event_mask(r0Open, EV_RLSD) &&
        dataOpen->eventMask == EV_RXCHAR && r0Open->eventMask == EV_RLSD);
    LeaveCriticalSection(&p.io);

    memset(&jData, 0, sizeof(jData)); jData.open = dataOpen; jData.done = CreateEvent(NULL, TRUE, FALSE, NULL);
    memset(&jR0, 0, sizeof(jR0));     jR0.open = r0Open;     jR0.done = CreateEvent(NULL, TRUE, FALSE, NULL);
    tData = CreateThread(NULL, 0, wait_worker, &jData, 0, NULL);
    tR0 = CreateThread(NULL, 0, wait_worker, &jR0, 0, NULL);
    begin = GetTickCount();
    while ((!dataOpen->eventWaiter || !r0Open->eventWaiter) && GetTickCount() - begin < 500) Sleep(1);
    check("both_waiters_blocked_concurrently", dataOpen->eventWaiter == 1 && r0Open->eventWaiter == 1);

    /* Duplicate waiter on SAME handle must still return ERROR_BUSY */
    memset(&jBusy, 0, sizeof(jBusy)); jBusy.open = r0Open;
    jBusy.ok = serial_wait_event_open(r0Open, (PBYTE)&jBusy.mask, 4, NULL);
    check("same_handle_second_waiter_busy", !jBusy.ok && GetLastError() == ERROR_BUSY);

    /* RXCHAR wakes AsyncMac ONLY; Unimodem WatchForDisconnect stays peacefully blocked */
    EnterCriticalSection(&p.io);
    m36_broadcast_events(&p, EV_RXCHAR);
    LeaveCriticalSection(&p.io);
    w = WaitForSingleObject(tData, 1000);
    check("rxchar_wakes_data_not_r0", w == WAIT_OBJECT_0 && jData.ok && jData.mask == EV_RXCHAR && WaitForSingleObject(tR0, 40) == WAIT_TIMEOUT);
    CloseHandle(tData);

    /* PURGE_RXCLEAR clears pending EV_RXCHAR on opens without touching r0 EV_RLSD waiter */
    EnterCriticalSection(&p.io);
    m36_broadcast_events(&p, EV_RXCHAR);
    check("purge_rxclear_clears_open_pending",
        dataOpen->eventPending == EV_RXCHAR &&
        m36_purge_host_buffers(&p, PURGE_RXCLEAR) &&
        dataOpen->eventPending == 0);
    LeaveCriticalSection(&p.io);
    check("purge_does_not_drop_r0_waiter", WaitForSingleObject(tR0, 30) == WAIT_TIMEOUT);

    /* CDC notify: baseline RLSD=1,DSR=1 -> no edge; then RLSD drops -> wakes r0 ONLY */
    pkt[8] = 0x03;
    EnterCriticalSection(&p.io);
    fired = 0;
    m36_update_notify(&p.notifyState, pkt, 10, 1, GetTickCount(), 0xFFFFFFFF, &fired);
    if (fired) m36_broadcast_events(&p, fired);
    LeaveCriticalSection(&p.io);
    check("baseline_notify_no_spurious_r0_wake", fired == 0 && m36_modem_status(&p.notifyState) == (MS_RLSD_ON | MS_DSR_ON) && WaitForSingleObject(tR0, 30) == WAIT_TIMEOUT);

    pkt[8] = 0x02; /* RLSD drops, DSR remains high */
    EnterCriticalSection(&p.io);
    fired = 0;
    m36_update_notify(&p.notifyState, pkt, 10, 1, GetTickCount(), 0xFFFFFFFF, &fired);
    if (fired) m36_broadcast_events(&p, fired);
    LeaveCriticalSection(&p.io);
    w = WaitForSingleObject(tR0, 1000);
    check("rlsd_drop_wakes_r0_and_status_low", w == WAIT_OBJECT_0 && jR0.ok && jR0.mask == EV_RLSD && !(m36_modem_status(&p.notifyState) & MS_RLSD_ON) && (m36_modem_status(&p.notifyState) & MS_DSR_ON) && dataOpen->eventPending == 0);
    CloseHandle(tR0);

    /* 3. SetCommMask(r0, 0) cancels r0 WatchForDisconnect cleanly (TSPI_lineDrop path) without touching dataOpen */
    ResetEvent(jR0.done);
    tR0 = CreateThread(NULL, 0, wait_worker, &jR0, 0, NULL);
    begin = GetTickCount();
    while (!r0Open->eventWaiter && GetTickCount() - begin < 500) Sleep(1);
    Sleep(20);
    EnterCriticalSection(&p.io);
    check("setmask_zero_on_r0_preserves_data_mask", m36_open_set_event_mask(r0Open, 0) && dataOpen->eventMask == EV_RXCHAR);
    LeaveCriticalSection(&p.io);
    w = WaitForSingleObject(tR0, 1000);
    check("setmask_zero_returns_mask0_ok", w == WAIT_OBJECT_0 && jR0.ok && jR0.mask == 0);
    CloseHandle(tR0);

    /* 4. Closing r0 while its waiter is active safely wakes waiter (refcount prevents UAF) */
    EnterCriticalSection(&p.io);
    m36_open_set_event_mask(r0Open, EV_RLSD);
    LeaveCriticalSection(&p.io);
    ResetEvent(jR0.done);
    tR0 = CreateThread(NULL, 0, wait_worker, &jR0, 0, NULL);
    begin = GetTickCount();
    while (!r0Open->eventWaiter && GetTickCount() - begin < 500) Sleep(1);
    Sleep(20);
    m36_open_release(r0Open);
    w = WaitForSingleObject(tR0, 1000);
    check("close_r0_wakes_blocked_waiter_safely", w == WAIT_OBJECT_0 && jR0.ok && jR0.mask == 0 && p.opened == 1 && p.refs == 2);
    CloseHandle(tR0);

    /* 5. Physical USB detach wakes remaining dataOpen waiter with ERROR_DEVICE_NOT_CONNECTED */
    ResetEvent(jData.done);
    tData = CreateThread(NULL, 0, wait_worker, &jData, 0, NULL);
    begin = GetTickCount();
    while (!dataOpen->eventWaiter && GetTickCount() - begin < 500) Sleep(1);
    Sleep(20);
    SetEvent(parent.stop);
    EnterCriticalSection(&p.io);
    m36_wake_all_opens(&p);
    LeaveCriticalSection(&p.io);
    w = WaitForSingleObject(tData, 1000);
    check("usb_detach_fails_waiter_disconnected", w == WAIT_OBJECT_0 && !jData.ok && jData.err == ERROR_DEVICE_NOT_CONNECTED);
    CloseHandle(tData);

    m36_open_release(dataOpen);
    check("final_refs_and_openlist_clean", p.refs == 1 && p.opened == 0 && p.openList == NULL);

    CloseHandle(jData.done);
    CloseHandle(jR0.done);
    CloseHandle(p.rxReady);
    CloseHandle(p.eventWake);
    CloseHandle(parent.stop);
    DeleteCriticalSection(&p.rxIO);
    DeleteCriticalSection(&p.io);

    printf("D41_HOST_TEST CHECKS=%lu FAILURES=%lu COMPLETE\n", (unsigned long)checks, (unsigned long)failures);
    return failures ? 1 : 0;
}
