# Issue/PR Management Bot Information

## Overview

The goal here is to help us automate, manage, and narrow down what we actually need to focus on in this repository.
We'll be using tags, primarily, to help us understand what needs attention, what is sitting around and turning stale, etc.

### Tagging/Process Details

1. When new issues arrive, or when issues are not properly tagged... we'll mark them as `Needs-Triage` automatically.
   - The core contributor team will then come through and mark them up as appropriate. The goal is to have a tag that fits the `Product`, `Area`, and `Issue` category. 
   - The `Needs-Triage` tag will be removed manually by the core contributor team during a triage meeting. (Exception, triage may also be done offline by senior team members during high-volume times.)
   - An issue may or may not be assigned to a contributor during triage. It is not necessary to assign someone to complete it.
   - We're not focusing on Projects yet.
1. When core contributors need to ask something of the author, they will manually assign the `Needs-Author-Feedback` tag.
   - This tag will automatically drop off when the author comes back around and applies activity to the thread.
   - When this tag drops off, the bot will apply the `Needs-Attention` tag to get the core contribution team's attention again. If an author cares enough to be active, we will attempt to prioritize engaging with that author.
   - If the author doesn't come back around in a while, this will become a `No-Recent-Activity` tag.
   - If there's activity on an issue, the `No-Recent-Activity` tag will automatically drop.
   - If the `No-Recent-Activity` stays, the issue will be closed as stale. 
1. PRs will automatically get a `Needs-Author-Feedback` tag when reviewers wait on the author
   - This follows a similar decay strategy to issues.
   - If the author responds, the `Needs-Author-Feedback` tag will drop.
   - If there is no activity in a while, the `No-Recent-Activity` tag will appear.
   - If the `No-Recent-Activity` tag exists for a while, the PR will be closed as stale.
1. Issues manually marked as `Resolution-Duplicate` will be closed shortly after activity stops
1. Pull requests manually marked as `AutoMerge` will permit the bot to complete the PR and do cleanup when certain conditions are met. See details below.

## Rules

### Issue Management

#### Mark as Triage Needed
- When an issue doesn't meet triage criteria, applies `Needs-Triage` tag.
- Triage criteria:
   - 
  
### Author Has Responded
- When an issue with `Needs-Author-Feedback` gets an author response, drops that tag in favor of `Needs-Attention` to flag core contributors to drop by.

### Remove Activity Tag
- When an issue with `No-Recent-Activity` has activity, drops this tag

#### Close Stale
- Every hour, checks if there's an issue with `Needs-Author-Feedback` and `No-Recent-Activity` for 3 days. Closes as stale.

#### Tag as No Activity
- Every hour, checks if there's been no activity in 4 days on an issue that `Needs-Author-Feedback`. If it's been 4 days, mark `No-Recent-Activity` as well.

#### Close Duplicate Issues
- Every hour, checks if there's been a day since the last activity on an issue with tag `Resolution-Duplicate` and closes it if inactive.

### PR Management

#### Codeflow Link *(Disabled)*
- Bumps a PR with a link to the Microsoft CodeFlow tool for reviewing PRs

#### Marks PR as Awaiting Author Feedback
- When a reviewer marks the PR as changes requested, the `Needs-Author-Feedback` tag will be applied

#### Removes Awaiting Author Feedback
- When the PR author updates the pull request, comments on it, or responds to a review, the `Needs-Author-Feedback` tag is removed.

#### Removes No Recent Activity
- When anyone touches the pull request, the `No-Recent-Activity` tag is removed.

#### Markup stale pull requests
- Every hour, if a pull request `Needs-Author-Feedback` and hasn't been touched in 7 days, it will get the `No-Recent-Activity` tag.

#### Close stale pull requests
- Every hour, if a pull request has `No-Recent-Activity` and hasn't been touched in a further 7 days, it will be closed.

#### Auto-Merge pull requests
- When a pull request has the `AutoMerge` label...
  - If it has been at least 480 minutes and all the statuses pass, merge it in.
  - Will use Squash merge stratgy
  - Will attempt to delete branch after merge, if possible
  - Will automatically remove the `AutoMerge` label if changes are pushed by someone *without* Write Access.

##### **ADD ADDITIONAL AUTO-MERGE RULES/TRIGGERS HERE**
  
## Admin Panel
https://fabric-cp.azurewebsites.net/bot/
