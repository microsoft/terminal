

# Windows Terminal Contribution Guide

Welcome! This guide provides instructions for reporting issues, proposing features, and submitting code contributions through Pull Requests (PRs).

## Development Workflow Overview

The Windows Terminal team works openly on this repository, handling issues, features, and code reviews publicly to ensure quality and transparency. This culture helps the community understand the project deeply.

### Issue Management Automation

We use a bot to manage issues and PRs—applying labels that automate reminders and handle stale items. Please monitor your GitHub notifications closely and respond promptly to avoid automatic closures.

## Reporting Issues and Feature Requests

**Security Issues:** Do not report security problems in public issues. See SECURITY.md for proper reporting procedures to Microsoft Security Response Center.

Before starting work:

- Search existing issues to avoid duplication.
- If none exist, file a new detailed issue including device specs, Windows build, tools used, reproduction steps, and error outputs.
- Use the appropriate templates and avoid non-informative comments such as “+1.” Use reactions to show support instead.

## Contributing Code

If you plan to fix or implement features, please communicate this in the relevant issue.

Start with beginner-friendly ["walkthrough"](https://aka.ms/terminal-walkthroughs) issues or filter for “Help Wanted” and “good first issue” labels.

### Issue Types

- **Bugs:** Fixes for existing but broken functionality.
- **Tasks:** Smaller new features or improvements.
- **Features:** Larger enhancements often requiring design specifications.

Some features require a written spec stored under `/doc/specs`. These specs are collaboratively reviewed before development.

## Development Steps

After agreeing on an approach or spec:

1. Fork the repo.
2. Clone your fork locally.
3. Create a feature branch.
4. Optionally, open a Draft PR early.
5. Develop and test your changes locally (see [build docs](./doc/building.md) and [TAEF testing](./doc/TAEF.md)).
6. Mark your PR as “Ready for Review” when ready.

## Code Review and Merge

PRs will be carefully reviewed, often with feedback cycles. Contributions may affect Windows Terminal, Windows Console, and downstream Windows components, so high standards apply.

Once approved, your PR will be merged and closed automatically.

***

Thank you for contributing! Explore current opportunities under our [Help Wanted](https://github.com/microsoft/terminal/labels/Help%20Wanted) label to get started.

***

Feel free to use this as your contributor guide text! Let me know if you want me to format it in markdown or any other style.