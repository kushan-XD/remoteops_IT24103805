# Checkpoint 2: minimal TCP connection

Environment: CentOS Stream 10.

Observed results:
- Agent listened on TCP port 9410; verified using ss.
- make -f Makefile_805 built the Controller without visible warnings.
- Controller connected to 127.0.0.1:9410 and returned exit status 0.
- Agent printed that it accepted and closed the test connection.
- Invalid IPv4 input was rejected with exit status 1.

Current limitations:
- Connections close immediately.
- Authentication, command handling and concurrent workers are not implemented.

AI assistance:
Codex supplied the initial Agent, Controller and Makefile increments.
I compiled and ran them in CentOS and shared screenshots for review.
My own explanation of the socket lifecycle is still to be recorded.
