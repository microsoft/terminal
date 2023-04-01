# Issue/PR Management Bot Information

## Overview

The goal here is to help us automate, manage, and narrow down what we actually need to focus on in this repository.
We'll be using tags, primarily, to help us understand what needs attention, what is sitting around and turning stale, etc.

### Quick-Guidance to Core Contributors
1. Look at `Needs-Attention` as top priority
1. Look at `Needs-Triage` during triage meetings to get a handle on what's new and sort it out
1. Look at `Needs-Tag-Fix` when you have a few minutes to fix up things tagged improperly
1. Manually add `Needs-Author-Feedback` when there's something we need the author to follow up on and want attention if they return it or an auto-close for inactivity if it goes stale.

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

### Triage Shorthand
- All rules in this category apply to triaging issues. They're shorthand comments that the triage team can use in order to complete the triage process faster. 
- Only individuals with `Write` or `Admin` privileges on the repository can use these responses.

#### Duplicate Issues
- When a comment on the thread says `/dup #<issue ID>`...
1. Reply with a comment explaining that the issue is a duplicate and recommend that the opener and interested parties follow the issue on the listed ID number.
1. Close the issue
1. Remove all `Needs-*` tags
1. Add `Resolution-Duplicate`

### Issue Management

#### Mark as Triage Needed
- When an issue doesn't meet triage criteria, applies `Needs-Triage` tag. Right now, this is just when it's opened.

#### Author Has Responded
- When an issue with `Needs-Author-Feedback` gets an author response, drops that tag in favor of `Needs-Attention` to flag core contributors to drop by.

#### Remove Activity Tag
- When an issue with `No-Recent-Activity` has activity, drops this tag

#### Close Stale
- Every hour, checks if there's an issue with `Needs-Author-Feedback` and `No-Recent-Activity` for 3 days. Closes as stale.

#### Tag as No Activity
- Every hour, checks if there's been no activity in 4 days on an issue that `Needs-Author-Feedback`. If it's been 4 days, mark `No-Recent-Activity` as well.

#### Close Duplicate Issues
- Every hour, checks if there's been a day since the last activity on an issue with tag `Resolution-Duplicate` and closes it if inactive.

#### Enforce tag system
- When an issue is opened or labels are changed in any way, we will check if the tagging matches the system. If not, it will get `Needs-Tag-Fix`. The system is to have an `Area-`, `Issue-`, and `Product-` tag for all open things, and also a `Resolution-` for closed ones.
- When the tags from appropriate categories are applied, it will auto-remove the `Needs-Tag-Fix` tag.
- `Resolution-Duplicate` is sufficient to fix all tagging. (`Area-`, `Issue-`, and `Product-` are not needed for a duplicate.)

#### Clean-up low quality issues
- If an issue is filed with an incomplete title...
- If an issue is filed with nothing in the body...
- If an issue is filed matching a pattern that happens all the time (common duplicate phrase, obvious multiple-issues-in-one pattern)...
- Then close the issue automatically informing the opener that they can resolve the problem and reopen the issue. (See Bug/Feature templates for example situations.)

#### Help ask for Feedback Hub
- When a comment on the thread says `/feedback`...
1. Then reply to the issue with a bit of text on asking the author to send us data with Feedback Hub and give us the link.
1. And add the `Needs-Author-Feedback` tag

#### Remove Help Wanted from In PR issues
- If an issue gets the `In-PR` tag when a new PR is created, we will remove the `Help-Wanted` tag to avoid someone trying to work on an issue where another person has already submitted a proposed fix.

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
  - Will use Squash merge strategy
  - Will attempt to delete branch after merge, if possible
  - Will automatically remove the `AutoMerge` label if changes are pushed by someone *without* Write Access.
  - More information on bot-logic that can be controlled with comments is [here](https://github.com/OfficeDev/office-ui-fabric-react/wiki/Advanced-auto-merge)

#### Mark issues with an active PR
- If there is an active PR for an issue, label that issue with the `In-PR` label

#### Add committed fix tag for completed PRs
- When a PR is finished and there's no outstanding work left on a linked issue, add the `Resolution-Fix-Committed` label

#### Remove Needs-Second from completed PRs
- If a PR is closed and it has the `Needs-Second` tag, the bot will remove the tag.

### Release Management

When a release is created, if the PR ID number is linked inside the release description, the bot will walk through the related PR and all of its related issues and leave a message.
- PR message: "🎉{release name} {release version} has been released which incorporates this pull request.🎉
- Issue message: 🎉This issue was addressed in #{pull request ID}, which has now been successfully released as {release name} {release version}.🎉"

## Admin Panel
[Here](https://portal.fabricbot.ms/bot/?repo=microsoft/terminal)
