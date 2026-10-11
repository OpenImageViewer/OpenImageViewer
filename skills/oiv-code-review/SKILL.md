---
name: oiv-code-review
description: Review a complete OIViewer/OpenImageViewer Git change for correctness, cross-file integration, completeness, risk, tests, and implementation quality. Use for pull requests, branches, commit ranges, patches, or staged and working-tree changes, including C++, tests, build configuration, workflows, and documentation.
---

# OIV Code Review

Review the change as one coherent unit, including relevant unchanged dependencies.
Use [OIV Code Writer](../oiv-code-writer/SKILL.md) as the implementation standard
for first-party C++ under OIVShared, OIVAppCore, OIVLib, Clients/OIViewer, and Tests.
Keep review methodology here and coding standards in that skill. For other file
types, use their existing repository contracts and conventions.

## Scope and Execution

- Establish intended behavior from the request, PR description, commits, contracts, and tests. Record uncertainty rather than inventing intent.
- Resolve committed boundaries to immutable identifiers. For PRs, prefer an immutable platform-provided diff base. Otherwise compute the merge base of the supplied target commit and head, and compare that merge base with head. Report the actual diff base and head, not just the target tip.
- For an explicit commit range, use its requested boundary. For a branch without one, use its upstream default branch's merge base. For pending changes, compare HEAD with the complete index and worktree, including relevant untracked files. For staged-only scope, compare HEAD with the index. For unstaged-only scope, compare the index with the worktree and include untracked files when requested. Record HEAD, the index tree, and mutable file presence and content hashes so changes can invalidate evidence.
- For patch-only reviews without a base tree, identify the ordered patch artifacts and limit conclusions to visible content. Disclose missing integration evidence and use `incomplete` when those gaps are material.
- Identify renames, copies, deletions, generated output, and external submodule changes. Treat generated and external content as integration evidence unless its source or generation process is in scope. Do not apply first-party C++ conventions to external code.
- Keep review read-only by default. Do not edit, format, stage, commit, publish comments, install tools, or change external state without separate task authorization. Run only permitted non-mutating checks. Builds, tests, and GUI smoke runs require task authorization and must respect existing build-directory guidance.
- Execution restrictions from the task take precedence over skill workflow steps. In CI, report concerns and limitations without requesting permission or attempting fixes.
- If a required skill or relevant dependency is unavailable, disclose the affected coverage. Do not claim a complete review when material evidence is missing.

## Evidence and Coverage

Maintain one internal evidence ledger and coverage map throughout the review.
Do not create scratch files unless requested. Scale detail to behavioral risk.

- Group coverage by behavior or data flow, including changed files, relevant unchanged dependencies, verification performed, and remaining gaps.
- Track each candidate's claim, exact base and current evidence, affected behavior, severity, confidence, attribution, proposed action, and status. Distinguish candidates from confirmed, dismissed, merged, or unresolved claims.
- Classify severity by impact: Critical for exploitable failures or data loss, High for likely core-path or broad compatibility failures, Medium for meaningful plausible defects or material maintenance costs, and Low for localized minor defects or gaps.
- Classify attribution as introduced, worsened, exposed, or pre-existing. Report existing problems only when the change depends on them, makes them reachable, or cannot safely ship without addressing them. Exclude unrelated existing weaknesses.
- Classify action independently from severity and attribution. A fix is required when leaving it unresolved makes the change incorrect, unsafe, incomplete, or materially difficult to maintain. An optional improvement needs a demonstrated bounded benefit while the current change remains sound.

## Ordered Review Passes

Perform these passes serially against the same evidence and coverage state.
Skip inapplicable checks and prioritize high-risk contracts over textual diff size.

1. **Intent and change map.** Read the full diff before analyzing individual areas. Identify behavioral slices, unchanged callers, analogous implementations, tests, and build definitions. Distinguish mechanical movement from semantic changes. Mark public APIs, shared data, ownership, synchronization, trust boundaries, and weakened tests as risk anchors.
2. **Behavioral correctness.** Trace control flow, validation, transformations, arithmetic, state transitions, failures, partial success, and edge cases. Compare base and current behavior against established intent and supported inputs, including removed behavior.
3. **Cross-file integration.** Trace producers, consumers, conversions, and initialization. Check ownership, lifetime, serialization, configuration, compatibility, synchronization, and side effects. Inspect unchanged integration points affected by changed contracts.
4. **Completeness.** Search for code that should have changed but did not. Check platform and renderer variants, registrations, build entries, analogous paths, tests, documentation, partial migrations, stale interfaces, and cleanup. Use repository searches and relevant history rather than inferring completeness from the diff.
5. **Risk and implementation quality.** Apply relevant security, concurrency, performance, resource, and compatibility checks. Use the coding skill where applicable. Identify redundant helpers, duplicated behavior, stale comments, dead state, and accidental complexity. For new shared abstractions or ownership and state changes, compare feasible repository patterns and simpler alternatives. Report alternatives only with concrete benefits and explicit compatibility, migration, lifetime, validation, or synchronization trade-offs. Distinguish explicit repository requirements from personal style preferences. Report requirement violations with their rule and concrete consequence, avoiding speculative rewrites or cosmetic preferences.
6. **Verification.** First check that test assertions prove the intended behavior and failure shape, then assess coverage of important cases. Run only authorized checks and record their outcomes. Separate inspected tests from executed tests, and distinguish unavailable runtime evidence from source-level conclusions.

## Challenge and Convergence

- Seek both supporting and contradicting evidence for every candidate. Re-check base behavior, callers, tests, contracts, repository precedent, ownership, severity, confidence, attribution, and actionability.
- Merge duplicated findings and dismiss disproven claims. Challenge design alternatives against intended scope and migration cost, dismissing preferences whose benefits are speculative or outweighed by risk.
- Revalidate the snapshot before another analysis cycle and before reporting. If content changed, invalidate affected evidence and restart from the earliest affected pass.
- Repeat only affected passes when a finding, action, verdict, or material coverage gap changes. Retain valid earlier evidence.
- Finish when actionable candidates are confirmed, merged, or dismissed and material gaps are disclosed. If relevant claims remain unresolved or material coverage cannot be established, mark the review incomplete while retaining confirmed findings.

## Report

Lead with one holistic verdict, scope, and actual reviewed snapshot boundary.
For committed reviews, identify the immutable diff base and head. For pending
changes, identify HEAD and the index/worktree scope, retaining mutable content
hashes internally for revalidation rather than flooding the report.
Present confirmed required findings by severity, followed by a short optional
improvements section when evidence supports it. For each finding include:

- Severity and confidence, exact file and line location, and attribution.
- Concrete trigger, affected behavior, consequence, and supporting evidence.
- Suggested action labeled required or optional. For an alternative design, state the current approach, bounded alternative, benefit, and material trade-offs.

Then summarize coverage by behavior, exclusions and gaps, verification performed,
remaining uncertainty, and convergence status. Keep reports concise and omit
empty optional sections, raw scratch notes, and numerical per-file ratings.
Do not manufacture cleanup suggestions to fill a section.

Choose exactly one verdict in this precedence order:

| Verdict | Meaning |
| --- | --- |
| `incomplete` | Material coverage or convergence is insufficient, overriding other verdicts while retaining confirmed findings. |
| `changes required` | Coverage and convergence are sufficient and a confirmed required finding remains. |
| `approve with follow-up` | No required finding remains, but evidence-backed optional or external follow-up exists. |
| `approve` | Coverage and convergence are sufficient with no required, optional, or external follow-up. |

Call a review clean only with `approve`. A static review can converge without
executing tests when source evidence establishes the relevant contracts. Disclose
unexecuted tests and runtime uncertainty, using `incomplete` only when the missing
evidence materially prevents a conclusion. Do not equate approval with a passing
build or a GitHub approval submission.
