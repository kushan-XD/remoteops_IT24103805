# Checkpoint 11: Controller UDP reception

The Controller binds the requested UDP port before sending
MONITOR START and receives updates in a separate thread.
Received updates are checked for the Agent's IPv4 address,
SYSINFO format and SID:5083.

MONITOR STOP stops and joins the local receiver after the Agent's
acknowledgement. Normal Controller exit also joins the receiver.

Verified interactively:
- Repeated UDP SYSINFO updates were displayed.
- MONITOR STOP returned OK MONITOR_STOPPED.
- QUIT returned OK BYE and the Controller returned to the shell.

Evidence: screenshot 14.png.
UDP output can interrupt the visible input prompt.

Two-Controller verification:
- Controllers used separate UDP ports, 9500 and 9501.
- After Controller B stopped, Controller C continued receiving updates.
- Controller C successfully requested TCP SYSINFO while monitoring.
- Both Controllers completed MONITOR STOP and QUIT.
Evidence: screenshots 15.png and 16.png.

Two-Controller verification:
- Controllers used separate UDP ports, 9500 and 9501.
- After Controller B stopped, Controller C continued receiving updates.
- Controller C successfully requested TCP SYSINFO while monitoring.
- Both Controllers completed MONITOR STOP and QUIT.
Evidence: screenshots 15.png and 16.png.
