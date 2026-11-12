# Contributing to bitcoinfuzz

Read this before opening a PR. Most PRs that get closed or reworked break one
of the rules below.

## What the project does

bitcoinfuzz feeds the same input to several Bitcoin and Lightning libraries and
aborts when they disagree. A disagreement is the result we are looking for: it
is either a bug in a library, an ambiguity in a spec, or a bug in our harness.

A module (`modules/<name>`) is a thin adapter: it passes the input to the
library's public API and formats the output so the driver can compare it. The
code under test must be the library's code.

## Rule 1: do not implement any logic in the harness

If a module contains validation or protocol logic that we wrote, the fuzzer
compares the other libraries against our code and can no longer find bugs in
that library. This is the most common reason PRs are rejected.

- **The library lacks an API the target needs.** Do not rebuild it from lower
  level pieces (merkle roots, witness commitments, signature checks, parsers).
  Leave that module out of the target, or get the API exposed upstream first.
- **The library accepts something the others reject (or the opposite).** Do
  not add a check in the module to make it agree. See
  [Handling a mismatch](#handling-a-mismatch).
- **Do not copy another library's rules into a module** so both sides agree.
  The target then passes while testing nothing.

Normalizing output is fine (hex encoding, field order, a default for an absent
field). Deciding whether an input is valid is not.

## Handling a mismatch

1. Reproduce it with a single input and `LOG_OUTPUTS=1` (see
   [Reproducing Crashes](./RUNNING.md#reproducing-crashes)).
2. Read the spec (BIP, BOLT) and decide who is wrong.
3. Act on the cause:
   - **Harness bug** (modules format the same value differently, call
     different APIs, use different flags or networks): fix the harness.
   - **Library bug or spec violation**: report it upstream, following the
     project's SECURITY.md if it may be security relevant. Then add a skip with
     a TODO (below) and add the report to the
     [list of bugs found](./README.md#bugsinconsistencesmismatches-found-by-bitcoinfuzz).
     If it was reported privately, the skip comment, the TODO and the list
     entry must not describe it until the fix is public.
   - **Spec is ambiguous**: open an issue on the spec, then skip with a TODO.
   - **Intended difference** (unsupported feature, stricter policy by design):
     skip with a comment. No TODO.

Never make a mismatch go away by changing what the module returns or by
stopping the mutator from generating the input.

## Ok, Fail and Skip

Every module returns one of three outcomes (`include/bitcoinfuzz/result.h`,
`ffi.h`, `ffi.rs`, `bfresult.py`, `BfResult.java`, `BfResult.cs`).

| Outcome | Meaning | Compared by the driver |
| --- | --- | --- |
| `Ok(value)` | The library accepted the input. | Yes |
| `Fail(reason)` | The library rejected the input. | Yes |
| `Skip()` | Leave this module out of the comparison for this input. | No |

`Fail` when another module returns `Ok` is a finding. `Skip` hides it, so a
skip needs a justification.

### When to skip

- A known divergence that is already reported upstream or intended.
- The module does not support the feature the input uses. This is the only
  case where a skip may cover a whole class of inputs.
- The input crashes or asserts in the library. A crash is a finding: report it
  upstream first, then skip with a TODO.

### When not to skip

- To silence a mismatch you have not investigated.
- For errors in the harness's own code (for example decoding the input before
  calling the library). Return `Fail`, so a library that accepts the same input
  is still noticed.

### How to skip

- **As narrow as possible.** Match the exact condition or error that diverges,
  not a whole class of inputs or a generic error. A broad skip removes the
  module from inputs where it still agrees with the others. For an unsupported
  feature, match the feature and nothing wider.
- **In the module of the library that diverges.** A skip in the driver removes
  every module from the comparison. A filter in the custom mutator only hides
  the input until the corpus contains it.
- **With a comment** that states who accepts, who rejects and what the spec
  says.
- **Remove skips that can no longer trigger.**

## TODOs

A `TODO` marks a temporary workaround that depends on an upstream change. It
tells a future reader what to delete and when. Each one states:

- what upstream is missing or does wrong,
- a link to the upstream issue or PR (open one if it does not exist),
- what to remove from the harness once it is fixed.

```go
// lnd rejects non-minimal feature vectors; the others accept them.
// TODO: drop this once the others reject them too, see
// https://github.com/lightning/bolts/pull/1341.
case errors.Is(err, bolt12.ErrNonMinimalFeatures):
	return bfSkip()
```

Do not add a TODO for an intended difference, and do not use TODOs for
unfinished work in your own PR.

## Adding a target

- **Compare something meaningful.** Return the parsed fields, not only
  accept or reject, when the libraries expose them.
- **Make sure the fuzzer can reach the interesting code.** If the input carries
  a signature or checksum, random mutations fail it in every module and all of
  them trivially agree. Such targets need a custom mutator
  (`custommutator/mutators/`) that re-signs or re-encodes after mutating, and
  that sometimes leaves the input invalid.
- **The mutator must not avoid inputs that cause mismatches.** It should
  produce anything a peer could send, including missing required fields and
  unknown types.
- **Every module must print the same value the same way.** Prefer the raw
  bytes from the wire over a library's own string form.
- Wire it everywhere: `include/bitcoinfuzz/basemodule.{h,cpp}`,
  `driver.{h,cpp}`, the modules, the matrix in
  `.github/workflows/workflow.yml` and a service in `docker-compose.yml`.
  A seed corpus goes to [bitcoinfuzz/corpora](https://github.com/bitcoinfuzz/corpora).

## Adding a module, or a module to a target

- Pin the library to a commit or version.
- Add the module's define to the target's `cxxflags` in
  `.github/workflows/workflow.yml` and in `docker-compose.yml`. A module that
  CI does not build will break unnoticed.
- New module: add `modules/<name>/README.md` with the build steps and a row in
  the table in `README.md`.

## Pull requests

- Search open issues and PRs first. Duplicated and overlapping PRs waste review
  time.
- One logical change per PR, based on `v2`.
- Every commit must build (CI builds commit by commit). Squash fixup commits.
- Run `make check-format-all` (`make format-all` to fix).
- Run the target against the matching corpus from
  [bitcoinfuzz/corpora](https://github.com/bitcoinfuzz/corpora) and fuzz it for
  a while. Say in the description what you ran and what you did not.
- Keep the description in sync with the code after a rework, and push before
  replying that a comment is addressed.
- Constants are `UPPER_SNAKE_CASE`. Comments explain why, in one or two lines.

## AI assisted contributions

AI tools are allowed. You are responsible for the PR: you must understand
every line, have built and run it, and be able to explain why the change
belongs in the harness and not upstream. PRs that read as unreviewed AI output
are closed without review.

Instructions for coding agents are in [AGENTS.md](./AGENTS.md).
