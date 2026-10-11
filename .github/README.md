# AI pull request review

The [AI code review workflow](workflows/ai-code-review.yaml) uses the
[Codex GitHub Action](https://developers.openai.com/codex/github-action)
to review new, reopened, and updated pull requests, including draft pull requests.
It reviews the complete diff from the merge base to the PR head and updates one PR comment.
The PR title and description provide intent context. New runs cancel earlier
reviews of the same PR.

Canonical skill instructions live in the shared root `skills/` directory.
The `.codex/skills` entrypoints retain discovery metadata and direct Codex to
read those instructions. Their `agents/openai.yaml` files hold Codex UI metadata.
These are ordinary pointer instructions rather than filesystem symlinks.
Edit the canonical files to change behavior. If a skill's name or description
changes, update its entrypoint metadata too.

The review prompt loads both entrypoints and their canonical instructions:

- [oiv-code-review](../.codex/skills/oiv-code-review/SKILL.md) supplies the complete
  review process for code, tests, build configuration, workflows, and documentation.
- [oiv-code-writer](../.codex/skills/oiv-code-writer/SKILL.md) supplies the coding
  standards for applicable first-party C++ changes.

Paths are relative to the checkout so both skills work on the Linux runner.
Skill guidance respects its declared scope and the workflow's read-only limits.
Reviews report a holistic verdict, required fixes, evidence-backed optional
improvements, coverage gaps, and verification limits. Static review approval does
not mean that tests passed and does not submit a GitHub approval. Missing required
skills, canonical files, or material evidence gaps produce an incomplete review.

## Manual runs

Open **Actions > AI code review > Run workflow**, choose `master` or
`oiv-code-review`, enter the PR number, and click **Run workflow**.
The PR must be open and its source branch must belong to this repository.
Manual and automatic runs share the review and comment publishing jobs.

## Setup

Add an OpenAI API key as the `OPENAI_API_KEY` repository secret under
**Settings > Secrets and variables > Actions**. The key needs API access and
available billing credit. Reviews consume OpenAI API usage.

The review job has read-only repository permissions and runs Codex in a
read-only sandbox with sudo removed. A separate job posts the review with
pull request write permission. Repository scripts and tests are not run.

Automatic fork pull requests are skipped because GitHub does not expose repository
secrets to their workflows. Manual runs also reject fork pull requests. The Codex
action restricts execution to users with repository write access by default. Bot-triggered runs require explicit
configuration of the action's `allow-bots` input for trusted bot accounts.

AI feedback is advisory. Builds and tests remain the validation for code changes.
