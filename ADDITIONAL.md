BitcoinFuzz Development Blueprint: Technical Deep Dive and Contribution Roadmap

1. Project Philosophy and the "Quality-First" Contribution Model

The strategic integrity of BitcoinFuzz is predicated on a contributor culture that prioritizes precision over raw velocity. In the high-stakes domain of Bitcoin protocol security, the primary currencies are technical reputation and respect for reviewer bandwidth. A rushed or suboptimal Pull Request (PR) is more than a technical failure; it is a breach of professional etiquette that signals a lack of regard for the cognitive load placed on maintainers. Our philosophy is clear: do not rush to open a PR simply to establish a presence. We prioritize deep engagement, where developers ask rigorous questions and refine their logic before submission, ensuring the review process remains an elite validation stage rather than a debugging session.

This "Quality-First" mindset is reinforced by a decentralized, open-source spirit regarding task management. We deliberately eschew formal issue assignment. Issues remain open to the community, and we do not grant "permission" to work on them. While this may lead to concurrent development on the same problem, we view this overlap as a strategic advantage. It fosters a natural peer-review environment where multiple solutions can be compared, ultimately increasing project velocity through the selection of the most robust implementation. This social contract ensures that only the most resilient code makes it into our security-critical infrastructure.

2. Testing Architecture: Driver Correctness and Property-Based Frameworks

The Driver Correctness Harness is the foundational assurance layer of the project. Because our fuzzers are tasked with identifying vulnerabilities in the Bitcoin protocol itself, the driver logic must be beyond reproach. Testing the fuzzer—a meta-requirement for system integrity—ensures that the harness remains deterministic and that custom mutators do not introduce false negatives or instability into the audit process.

A primary competency task involves the extendedkey.h mutator. This requires implementing a validation suite covering three mandatory properties. A critical implementation decision for contributors is whether to utilize the existing vendored decoders in the custommutator directory or introduce a minimal, standalone implementation for property validation; the goal remains minimizing external bloat while ensuring absolute correctness.

Mandatory Property	Technical Justification
Output Size Limits	Ensures mutator output does not exceed max_size, preventing buffer overflows and memory exhaustion in the target.
Base58Check Decoding	Validates that the mutator correctly generates or handles bytes that decode to the standard 78-byte extended key format.
Empty Input Handling	Guarantees stability and prevents crashes when the mutator is presented with null or empty input buffers.

We are aggressively shifting from "Onion Mutator" style testing to a rigorous Property-Based Testing (PBT) framework. By utilizing randomized input generation loops over static test vectors, we can identify edge cases in adversarial environments that traditional unit tests fail to reach. The driver.cpp contract, enforced via the assert(res == lastresponse) check, ensures deterministic behavior across the system. To further support cross-module consistency, we are introducing a MockBaseModule for the driver harness, allowing us to isolate and validate driver logic against simulated module responses.

3. Infrastructure, Orchestration, and Monitoring

Managing a polyglot fuzzing environment requires sophisticated automation to maintain long-running campaigns. We have identified a state of "Target Drift" that must be reconciled: currently, the canonical Driver and Docker Compose configurations support 29 targets, while the Continuous Integration (CI) environment (workflow.yml) is restricted to 27. Specifically, the addrv2 and transaction_eval targets are currently missing from the CI pipeline and must be aligned to ensure environment parity.

To manage this, we are developing an Infrastructure Automation & Monitoring Dashboard. The architecture centers on a FastAPI orchestrator MVP designed for programmatic target discovery and campaign lifecycle management. Key architectural debates involve the implementation of metrics collection; we are evaluating the use of "sidecar" containers versus integrated container metrics to monitor resource health. Furthermore, the orchestrator must support two distinct operational scopes:

1. Intersection Mode (Safe): Runs only the verified subset of targets known to be stable in production.
2. Union Mode (Full Coverage): Attempts to run the complete set of 29 canonical targets to maximize reach.

Long-term campaign stability also relies on rigorous corpus management. We strictly distinguish between "raw" and "optimized" corpus sizes. Without utilizing the -merge_control_file parameter, campaigns will inevitably exhaust disk space due to unbounded growth. By maintaining independent targets and corpora—such as the stump_modify_add target utilizing the stump_modify corpus—we can leverage shared seed sets across similar logic while maintaining granular execution control.

4. Polyglot Integration: Build System Nuances and Language "Gotchas"

BitcoinFuzz integrates C++, Rust, Go, and Python, necessitating a unified build system where strict adherence to linker-level symbol management is mandatory to prevent collisions in the unified binary. A primary example is the "secp256k1 Submodule Issue" in auto_build.py. Certain CUSTOMMUTATOR flags failed to initialize the secp256k1 submodule, leading to directory navigation failures; this is resolved by explicitly mapping CUSTOM_MUTATOR_ONION within the SUBMODULES_BY_FLAG configuration.

Language-specific build requirements are non-negotiable for system stability:

* Go Modules: Must invoke rename_cgo_symbols.sh within their respective Makefiles to resolve duplicate symbol conflicts during the final link stage.
* Python Modules: Require the inclusion of PYTHON_LDFLAGS in the root Makefile to ensure modules like pycoin and python-hdwallet link correctly with the C++ driver.

This polyglot approach enables high-impact differential fuzzing. By integrating multiple implementations of the same protocol logic—specifically targeting the bip32_master_keygen function using both pycoin and python-hdwallet—we can identify inconsistencies between libraries. If two distinct implementations produce divergent outputs for the same input, it indicates a potential bug in the logic or the specification itself.

5. Emerging Frontiers: MCP Servers and Agentic Fuzzing

The project is evolving toward "AI-readiness" via the Model Context Protocol (MCP). By exposing targets, coverage metrics, and the bug corpus as queryable resources, we transform the contributor experience into an agent-augmented workflow. A critical technical distinction in the MCP server implementation is the categorization of "Custom Mutators" (e.g., BOLT11). By isolating mutators from standard modules—perhaps via a specialized listcustommutators tool—we enable AI agents to understand specific corpus formatting requirements, such as the distinction between Bech32 strings and raw bytes, before they attempt to analyze a specific target.

6. Contributor Readiness: Practical Setup and Etiquette

While the technical barrier is high, the project maintains a transparent entry path. New developers are expected to be self-starters, leveraging the existing README.md, RUNNING.md, and SECURITY.md as their primary guides.

Pre-Flight Checklist for New Contributors:

* Environment Initialization: Clone the repository and perform a full submodule initialization.
* Target Validation: Use docker-compose to run targets locally and verify libFuzzer behavior and corpus persistence.
* Build Integrity: Ensure auto_build.py correctly handles any new flags or submodule dependencies.
* CI Alignment: Verify that new targets or changes are reflected consistently across Docker Compose and the GitHub Actions workflow.yml.

Standard Open-Source PR Etiquette:

* Reviewer Respect: Prioritize code clarity and deterministic testing; never submit unverified or "exploratory" code as a final PR.
* Transparency: Share draft branches early once they compile to allow for architectural alignment.
* Decentralized Collaboration: Embrace the no-assignment policy; view overlapping work as an opportunity for peer review rather than competition.

Contributing to BitcoinFuzz is a high-impact endeavor. You are not just writing code; you are engineering the automated defenses that secure the global Bitcoin ecosystem.


Technical Roadmap: Module Contributor Kit Competency Test

1. Project Mission and Community Standards

The Module Contributor Kit is the bedrock of the BitcoinFuzz ecosystem, designed to formalize the integration process for new libraries and fuzzing targets. As a Senior Engineer here, my focus is not just on whether your code compiles, but on its "review-readiness." In this project, a contribution is a reflection of your technical rigor and respect for the maintainers' time.

The mandate is simple: Quality over Speed. A rushed contribution that lacks self-review or ignores project standards creates a technical debt that slows the entire community.

"Just a tip for everyone. Do not be in a hurry to open a PR and have something in bitcoinfuzz. A rushed PR may destroy your reputation since it shows you had no care with the reviewers' time." — brunoerg, Project Maintainer

2. Task 1: Foundation – The CONTRIBUTING.md Stub

The first requirement of the competency test is the creation of a CONTRIBUTING.md file within your module directory. This file must explicitly define the naming conventions required by our automation logic. Specifically, the getmodule_dir() function in our build system expects a directory name that is lowercase and hyphen-consistent. If the module is Gocoin, the directory and all internal references must strictly follow the gocoin pattern.

Required Module File Checklist:

* [ ] Module-specific Makefile for local library compilation.
* [ ] Implementation source files (C++, Rust, Go, or Python).
* [ ] Standardized CONTRIBUTING.md (detailing naming and setup).
* [ ] Property-based tests for mutator logic (addressing the three-point safety check: size limits, decoding integrity, and empty input resilience).
* [ ] Integration with testonionmain.cpp or similar reference structures to validate the driver contract.
* [ ] Entry in auto_build.py under the correct SUBMODULES_BY_FLAG category.

Standard Directory and Naming Structure:

bitcoinfuzz/
├── modules/
│   └── [module_name]/           # Must be lowercase (e.g., 'pycoin'), matches getmodule_dir()
│       ├── Makefile             # Language-specific build instructions
│       ├── [module_name].cpp    # Primary integration logic
│       └── CONTRIBUTING.md      # Module-specific stub
├── docs/
│   └── CONTRIBUTING.md          # Global guidelines
└── Makefile                     # Root build system


3. Task 2: Ground Truth – The 23-Module Audit

To ensure the project maintains a "Single Source of Truth," you must conduct a comprehensive audit of the existing 23 modules. This data will be recorded in docs/module-matrix.md. Note the distinction: while there are 23 distinct modules, they may represent up to 29 individual fuzzing targets.

Module Reference Matrix (Template):

Module Name	Language	Integration Type	Status
Gocoin	Go	Wrapper	Active
Pycoin	Python	Wrapper	Active
MuSig2	C++	Direct	Active
[Name]	[Lang]	[Direct/Wrapper]	[Active/Blocked/CI-Only]

Verification Instructions:

1. Status Identification: To determine if a module is "Blocked," inspect the docker-compose.yml file. Look for targets where CXXFLAGS are commented out or missing; these often indicate known bugs that prevent execution.
2. Configuration Check: Verify module presence in docker-compose.yml (for local runs) versus workflow.yaml (for CI).
3. Active Definition: A module is "Active" only if it is functional in both environments. If it exists only in the driver but fails in CI, mark it as "Blocked (Buggy)."

4. Task 3: Execution – Annotated Fuzzing Walkthrough

You must provide a documented transcript of your first fuzzing session. This walkthrough demonstrates that your environment is correctly configured and that you understand corpus optimization.

Annotated Session Template:

docker-compose up [target_name] [Observe the libFuzzer output for execution speed (exec/s). A successful start should show a non-zero rate, indicating the driver is correctly passing data to the module.]

ls -l ./fuzz_data/[target_name]/corpus [Verify corpus growth. New interesting inputs generated by the mutator should appear as unique files in this directory. Ensure the file count increases over time.]

-merge_control_file=${FUZZ_DATADIR}/merge [LibFuzzer uses this control file to manage periodic corpus merging. Document this specifically to show how we prevent "sudden drops" in corpus growth metrics by distinguishing between raw input discovery and the optimized corpus set.]

5. Technical Requirements: Language-Specific Gotchas

Different languages introduce unique linking and symbol conflicts. Your competency test must demonstrate an understanding of these hurdles.

Go Modules

When building Go-based modules, you must include a call to rename_cgo_symbols.sh within the module's specific Makefile. This prevents duplicate symbol conflicts that arise when multiple modules using CGO are linked into the same environment.

Python Modules

For Python integrations, the root Makefile (not the module Makefile) must be updated to include PYTHON_LDFLAGS. This ensures the global linker can resolve the Python headers and library dependencies required by the driver.

6. Environment Context: Resolving Configuration Drift

A major task for the Contributor Kit is narrowing the drift between execution environments. We currently operate in two modes: "Union Mode" (full coverage) and "Intersection Mode" (safe/CI-only).

Environment vs. Target Count/Status:

Environment	Target Count	Notes
Driver / Compose	29	The canonical set. "Union Mode" targets everything registered.
GitHub Actions (CI)	27	"Intersection Mode." Currently missing addrv2 and transaction_eval.

Target vs. Corpus Mapping: In our CI environment, the relationship between a target and its corpus is independent. For example, the stump_modify_add target correctly utilizes the stump_modify corpus. You must ensure your module supports this independent mapping rather than assuming a strict 1:1 name match between target and corpus.

7. Submission Checklist and Proposal Alignment

Before submitting your competency test as part of a Summer of Bitcoin proposal, verify the following:

1. Submodule Initialization: Confirm that auto_build.py has been updated to include CUSTOM_MUTATOR_ONION (and any other relevant custom flags) in the SUBMODULES_BY_FLAG list. This prevents the common cd: can't cd to ../external/ build failure.
2. Driver Contract Integrity: Ensure all module logic respects the driver.cpp contract: assert(res == lastresponse). Any deviation indicates a failure in cross-implementation consistency.
3. The Three-Point Safety Check: Validate that your mutator logic passes:
  * Size Limit: Output must be ≤ max_size.
  * Decoding Integrity: For example, ensuring base58check decodes to the expected byte length (e.g., 78 bytes).
  * Resilience: Zero crashes on empty or null input.
4. Drift Mitigation: Verify that your PR does not introduce new discrepancies. Aim for "Union Mode" (29 targets) to ensure maximum coverage across Docker and the Driver.

"Module Contributor Kit" — A First-Class Onboarding System for Adding New Bitcoin Library Integrations to bitcoinfuzz
Every bug in the README's 40+ found-bugs list represents a Bitcoin library that was successfully integrated into bitcoinfuzz. The question is: why aren't there 50 modules instead of 23? The answer is integration friction. There is no CONTRIBUTING.md, no scaffold tooling, no language-specific FFI guide, and no CI guardrail validating that a new module is wired up correctly across all the files it must touch.

Developer
Expected Outcomes

CONTRIBUTING.md — six-file addition checklist, naming convention table, flag → class → directory mapping rules. Merged as PR 1.
docs/adding-a-module/ — language-specific FFI guides: rust.md, go.md, java.md, python.md, dotnet.md, c.md. Each guide includes a minimal worked example using an existing module as reference. Merged across PRs 2–4.
scripts/newmodule.py — scaffold generator: takes --name, --lang, and --targets flags; emits all six file stubs with inline TODO markers; adds the correct entries to Makefile, autobuild.py, and docker-compose.yml; prints a checklist of remaining manual steps. Tested with pytest for each supported language. Merged as PRs 5–6.
CI structural linter — a GitHub Actions job (or extension of existing CI) that, on any PR touching modules/, checks: module directory has module.h, module.cpp, Makefile; a corresponding flag block exists in the root Makefile; an entry exists in auto_build.py; at least one docker-compose.yml service compiles with the module's flag. Definition of done: zero false positives on all 23 existing modules. Merged as PR 7.
Reference example module — a minimal, heavily-commented "example" module (wrapping a trivially simple C library like libbase58 or a stub) that lives at modules/example/ and is the canonical teaching artifact linked from CONTRIBUTING.md. Merged as PR 8.
README quickstart section — "Add your Bitcoin library to bitcoinfuzz in under 30 minutes" — a numbered 10-step guide at the top of README.md, with a link to the full contributor kit. Merged as PR 9.
Adoption feedback artifact — a GitHub Discussions thread or pinned issue titled "Which library should we add next?" + direct outreach messages sent to ≥3 downstream library maintainers, with responses documented. This is the feedback loop deliverable.
Skills Required

-

Resources

-

Difficulty

Medium
Mentors

Bruno

Competency Test (to be submitted along with proposal)

Small PR: Add a CONTRIBUTING.md stub with the six-file checklist table, the flag-to-directory naming convention (derived from autobuild.py's getmodule_dir()
Measurement/feedback artifact: Audit all 23 existing modules and produce a Markdown matrix table (docs/module-matrix.md) mapping each module to: language runtime, FFI mechanism, which BaseModule targets it implements, and which docker-compose.yml services compile it. This table becomes the ground truth for the scaffold generator and proves the intern understands the full system.
User-facing doc/demo improvement: Extend RUNNING.md with an annotated "first fuzzing session" walkthrough — running just run script end-to-end, explaining what each libFuzzer output line means, and showing how to inspect a crash artifact in the docker/ volume. This is a zero-risk, immediately mergeable doc improvement that directly reduces the "I followed the 