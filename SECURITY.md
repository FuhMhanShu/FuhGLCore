# Security Policy

## Reporting a vulnerability

Please report security vulnerabilities **privately** via GitHub's
[private vulnerability reporting](https://github.com/FuhMhanShu/FuhGLCore/security/advisories/new).
Do **not** open a public issue, pull request or discussion for a security bug.

Include, if possible:

- the affected version/commit and platform (OS, GPU/driver, GL version),
- a description of the issue and its security impact,
- a minimal reproduction (code, GLSL source or input data that triggers it).

You can expect an acknowledgement within 72 hours. For confirmed issues we aim
to publish a fix or mitigation within 30 days and credit the reporter (unless
they prefer to stay anonymous).

## Supported versions

Security fixes are applied to the current state of `main` and released with the
next version (see [CHANGELOG.md](CHANGELOG.md); the project is currently in the
`0.x` line, so there is no older release line receiving fixes).

## Scope

FuhGLCore is a client-side rendering library embedded into an application. It
processes GLSL sources, image data and window-system events supplied by the
embedding application. A bug that requires the host application to feed the
library input it already fully controls (e.g. a hand-written shader string) is
generally a bug in the host application, not a vulnerability of this library.
Memory-safety and robustness bugs in library code itself — out-of-bounds
reads/writes, use-after-free, unbounded recursion or unbounded memory growth
(e.g. via `Shader::resolveIncludes`) — are always treated as vulnerabilities.
