
# Branches in Openconsole

In Openconsole, `dev/main` is the master branch for the repo.

Any branch that begins with `dev/` is recognized by our CI system and will automatically run x86 and amd64 builds and run our unit and feature tests. For feature branches the pattern we use is `dev/<alias>/<whatever you want here>`. ex. `dev/austdi/SomeCoolUnicodeFeature`. The important parts are the dev prefix and your alias.

`inbox` is a special branch that coordinates Openconsole code to the main OS repo.

The code will be checked into the OS repo at `/onecore/windows/core/console/open`. It would be prudent to make sure that directory builds in razzle with your submitted changes.

# Code Submission Process

Because we build outside of the OS repo, we need a way to get code back into it once it's been merged into `dev/main`. This is done by cherry-picking the PR to the `inbox` branch once it has been merged (and preferably squashed) into `dev/main`. We have a tool called Git2Git that listens for new merges into `inbox` and replicates the commits over to the OS repo. Feel free to approve and complete the `inbox` PR yourself. About a minute after the `inbox` PR is submitted, Git2Git will create a PR in the OS repo under the alias `miniksa`. It will automatically target the OS branch we're using at the time, it just needs you to go approve and complete it. Once that merge is completed it is a good idea to build the OS branch with the new code in it to make sure that the PR won't be the cause of a build break that evening.

## What to do when cherry-picking to inbox fails

Sometimes VSTS doesn't want to allow a cherry pick to the inbox branch. It might have a valid reason, or it might just be finicky. You'll need to complete the merge manually on a local machine. The steps are:

1. make sure you have pulled the latest commits for the `dev/main` and `inbox` branches
2. make a new branch from inbox
3. cherry-pick the commits from the PR to the newly created branch (this is easier if you squashed your commits when you merged into `dev/main`
4. fix any merge conflicts and commit
5. push the new branch to the remote
6. create a new PR of that branch in `inbox`
7. complete PR and continue on to completing the auto-created PR in the OS repo
