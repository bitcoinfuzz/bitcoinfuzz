# Instructions for coding agents

Read [CONTRIBUTING.md](./CONTRIBUTING.md) in full before changing anything. It
is the source of truth. The rules below are the ones agents break most often.

1. **Modules are thin adapters.** Call the library's public API and format its
   output. Never write validation, parsing, hashing or signature logic in
   `modules/`, `driver.cpp` or `helpers/` to stand in for something a library
   does not do or does not expose.
2. **A mismatch between modules is a finding, not a failing test.** Do not
   make it pass. Reproduce it, read the BIP or BOLT, and report to the user
   which library is wrong and why.
3. **Never change a module's result to match another library.** If the library
   diverges, the fix belongs upstream. In the harness the only options are a
   narrow `Skip` in that library's module, with a comment and a `TODO` linking
   the upstream issue, or nothing.
4. **Do not restrict a custom mutator** so it stops producing inputs that
   cause mismatches.
5. **Do not open upstream issues, comment on PRs or push on your own.** Draft
   the report and hand it to the user. This overrides the steps in
   CONTRIBUTING.md that say to report upstream: those are for the human.

Stop and ask the user when:

- making a target work for a module would require logic the library does not
  provide,
- you are about to add a `Skip` and cannot state who accepts, who rejects and
  what the spec says,
- a skip would cover more inputs than the exact divergence.

Before finishing:

- `make check-format-all`
- build the affected modules and replay the target's corpus,
- new module or target: updated `.github/workflows/workflow.yml`,
  `docker-compose.yml` and the READMEs,
- report what you ran and what you could not run. Do not claim tests you did
  not execute.
