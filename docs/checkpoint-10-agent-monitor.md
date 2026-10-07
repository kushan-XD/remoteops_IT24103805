# Checkpoint 10: Agent UDP monitoring

Each TCP session owns an independent joinable monitor thread.
MONITOR START sends SYSINFO datagrams to the TCP peer's IPv4 address
and the requested UDP port, approximately every two seconds.

MONITOR STOP signals and joins the worker. Session exit also stops
and joins it before releasing its state. Repeated START replaces
that session's previous monitor. Re-authentication stops monitoring.

Verified by tests/monitor_checkpoint.py:
- Authentication is required.
- Invalid ports are rejected.
- Repeated UDP updates contain SYSINFO values and SID:5083.
- TCP SYSINFO works while monitoring.
- STOP ends updates.
- QUIT and TCP EOF clean up monitoring.

Concurrent session isolation, repeated START and re-authentication
cleanup still require dedicated tests.
Controller UDP reception is not yet implemented.
