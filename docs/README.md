# Documentation Index

| Document | Purpose | Updated |
|---|---|---|
| [`../CLAUDE.md`](../CLAUDE.md) | Architecture rules, RAM budgets, safety invariants (source of truth) | by the user |
| [`LICENSING.md`](LICENSING.md) | Allowed and forbidden sources, dependency licenses | when a source or dependency changes |
| [`RAM_BUDGET.md`](RAM_BUDGET.md) | RSS estimates against the 260 MB target | when measured (Step 4) |
| [`AUDIT.md`](AUDIT.md) | Audit findings with status | end of every wave |
| [`CHANGELOG.md`](CHANGELOG.md) | Dated record of changes | every change |
| [`AGENTS.md`](AGENTS.md) | Agent task plan: dependable and non-dependable tracks, waves | when the plan changes |

## Keeping the docs current

- Every change adds a line under `[Unreleased]` in `CHANGELOG.md`. When a release is cut, move those lines under a dated version heading.
- `AUDIT.md`: update the status column in place and add new findings with the next ID. Do not delete old findings.
- `LICENSING.md`: add every new source or dependency before it is used.
- Dates are absolute (YYYY-MM-DD).
