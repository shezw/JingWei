---
name: create-skill
description: Guide for creating a new skill in the system.
---

To create a new skill, follow these steps:

1. create a new folder in the `.github/skills` directory with the name of your skill (e.g., `my-new-skill`).
2. Inside the new folder, create a `SKILL.md` file that contains SKILL.md files are Markdown files with YAML frontmatter. In their simplest form, they include:
YAML frontmatter
- name (required): A unique identifier for the skill. This must be lowercase, using hyphens for spaces. Typically, this matches the name of the skill's directory.
- description (required): A description of what the skill does, and when Copilot should use it.
- license (optional): A description of the license that applies to this skill.
A Markdown body, with the instructions, examples and guidelines for Copilot to follow.
