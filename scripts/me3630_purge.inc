/* Caller holds port->io. Clear host queues; do not retract issued USB data. */
static BOOL m36_purge_host_buffers(PORT *port,DWORD flags)
{
 if(!flags || (flags&~(PURGE_RXCLEAR|PURGE_RXABORT|PURGE_TXCLEAR)))return FALSE;
 if(flags&PURGE_TXCLEAR){port->txClearEpoch++;port->txQueued=0;}
 if(flags&PURGE_RXCLEAR){port->qHead=port->qCount=0;port->eventPending&=~EV_RXCHAR;ResetEvent(port->rxReady);if(!(port->eventPending&port->eventMask))ResetEvent(port->eventWake);}
 if(flags&PURGE_RXABORT){port->rxAbortEpoch++;SetEvent(port->rxReady);}
 return TRUE;
}
