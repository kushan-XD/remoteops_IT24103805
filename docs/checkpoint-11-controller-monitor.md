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
