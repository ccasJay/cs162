---
title: "Socket"
source: "https://cs162.org/static/hw/hw-http/docs/tasks/socket/#socket"
author:
published:
created: 2026-05-13
description: "HTTP Server"
tags:
  - "clippings"
---
Finish setting up the server socket in the `serve_forever` function.

- Bind the socket to an IPv4 address and port specified at the command line (i.e. `server_port`) with the `bind` syscall.
- Afterwards, begin listening for incoming clients with the `listen` syscall. At this stage, a value of 1024 is sufficient for the `backlog` argument of `listen`. When load testing in [Performance](https://cs162.org/static/hw/hw-http/docs/tasks/performance/), you may play around with this value and comment on how this impacts server performance.

After finishing this part, `curl` should output ”Empty reply from server”.


