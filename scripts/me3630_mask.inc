/* Caller holds port->io. Retain only still-subscribed historical events.
 * Do not recreate consumed RXCHAR merely because unread bytes remain. */
static BOOL m36_set_event_mask(PORT *port,DWORD mask)
{
 if(!m36_event_mask_supported(mask))return FALSE;
 port->eventMask=mask;port->eventEpoch++;port->eventPending&=mask;
 SetEvent(port->eventWake);return TRUE;
}
