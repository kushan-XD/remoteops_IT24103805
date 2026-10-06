# Checkpoint 3: interactive Controller and concurrent Agent

Observed results:
- The C Controller accepted keyboard commands and displayed AUTH/QUIT replies.
- The Agent compiled with -pthread without visible compiler warnings.
- Five connections were established simultaneously on TCP port 9410.
- The ss listing showed all five connections owned by Agent process 8335.
- The Agent printed five independent-worker messages.
- Fragmented AUTH and following QUIT passed the regression test.
- A new connection started unauthenticated in the regression test.

Evidence:
- 5.png: five established Agent-side connections.
- 6.png: five worker-assignment messages.
- Separate screenshot: build and authentication regression results.


Design:
Each worker receives its own allocated socket argument.
The worker frees that argument, handles the session and closes its socket.
Authentication and receive-buffer state are separate for each session.

AI assistance:
Codex supplied the Controller and concurrency patches.
I applied them and supplied screenshots of the observed results.
