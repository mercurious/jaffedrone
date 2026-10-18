# AGENTS.md

Jaffedrone is a 4-string drone synthesizer for the Miyoo Mini Plus running OnionOS.
`make` cross-compiles `dist/Jaffedrone/jaffedrone` with zig (ARMv7 hard-float, glibc 2.28)
against Onion's own SDL 1.2; `make test` renders screenshots and audio on the host into
`build/test`. The platform layer (gfx, font, SDL front-end, build) is shared in spirit with
the sibling HI5 project; the engine is its own.

## Git identity firewall

The maintainer has two GitHub identities that must never cross. Each repo belongs to exactly
one of them, and the owner of its `origin` remote decides which. Every commit, push, issue,
PR and doc in this repo uses this repo's identity only.

**Lane: `mercurious` (pseudonymous).** This repo is `mercurious/jaffedrone` (private).

- Commit as `mercurious <dutch.money@gmail.com>`. That is this repo's local `user.email`.
- Keep the maintainer's real name, institution and other accounts out of everything here:
  code, docs, commit messages, issues and PRs. Nothing in this repo may link this identity to
  the other one.
- `gh` usually has the other account active. Before pushing, or if a push fails with
  "Repository not found" or 403, run `gh auth switch --user mercurious`. When you're done,
  switch back to the account that `gh auth status` showed as active before.

On the maintainer's machines, a Claude Code `PreToolUse` hook enforces this:
`~/.claude/hooks/git-identity-firewall.py`, registered on `Bash` in `~/.claude/settings.json`.
It runs before `git commit|merge|rebase|cherry-pick|am|revert|push` and before
`gh repo create --push` and `gh pr create`, reads the origin owner and the configured
identity, and denies the command when they don't belong to the same lane. A second firewall
runs inside git itself via the global `core.hooksPath`
(`~/.config/git/identity-firewall/hooks`), covering every tool. Git has no global identity
(`user.useConfigOnly`), so each repo needs its own `user.name` and `user.email`
(`git config --local`). Never pass `--no-verify` or change `core.hooksPath`; only the
maintainer may bypass a block.
