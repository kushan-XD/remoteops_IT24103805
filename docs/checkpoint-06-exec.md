# Restricted EXEC checkpoint

Implementation:
- Permits exactly DATE, UPTIME, DISKFREE, HOSTNAME and WHOAMI.
- Selects fixed command strings; never interpolates client input.
- Flattens command output to a single bounded response line.
- Marks truncated output and drains the command's remaining output.

Observed results:
- EXEC before authentication was rejected.
- All five permitted operations returned successful tagged responses.
- HOSTNAME matched the local uname -n result.
- An initially mistyped HOSTNMAE was correctly rejected.
- Unknown names, shell-looking input, lowercase names, extra arguments
  and a missing name were rejected.
- tests/text_commands.py passed framing and whitelist checks.
- Authentication regression tests passed.

AI assistance:
Codex supplied the integration patch and tests.
I ran the commands and shared actual output for review.
