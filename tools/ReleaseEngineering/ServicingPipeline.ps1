# Copyright (c) Microsoft Corporation.
# Licensed under the MIT license.

#Requires -Version 7
#Requires -Modules PSGitHub

[CmdletBinding()]
Param(
    [string]$Version,
    [string]$SourceBranch = "origin/main",
    [switch]$GpgSign = $False
)

Function Prompt() {
    "PSCR {0}> " -f $PWD.Path
}

Function Enter-ConflictResolutionShell() {
    $Global:Abort = $False
    $Global:Skip = $False
    $Global:Reject = $False
    Push-Location -StackName:"ServicingStack" -Path:$PWD > $Null
    Write-Host @"
`e[31;1;7mCONFLICT RESOLUTION REQUIRED`e[m

"@

    & git status --short

    Write-Host (@"

`e[1mCommands`e[m
`e[1;96mdone  `e[m Complete conflict resolution and commit
`e[1;96mskip  `e[m Skip this commit
`e[1;96mabort `e[m Stop everything
`e[1;96mreject`e[m Skip and `e[31mremove this commit from servicing consideration`e[m
"@)
    $Host.EnterNestedPrompt()
    Pop-Location -StackName:"ServicingStack" > $Null
}

Function Done() {
    $Host.ExitNestedPrompt()
}

Function Abort() {
    $Global:Abort = $True
    $Host.ExitNestedPrompt()
}

Function Skip() {
    $Global:Skip = $True
    $Host.ExitNestedPrompt()
}

Function Reject() {
    $Global:Skip = $True
    $Global:Reject = $True
    $Host.ExitNestedPrompt()
}

Function Set-GraphQlProjectEntryStatus($Project, $Item, $Field, $Value) {
    Invoke-GitHubGraphQlApi -Query 'mutation($project: ID!, $item: ID!, $field: ID!, $value: String!) {
        updateProjectV2ItemFieldValue(
            input: {
                projectId: $project
                itemId: $item
                fieldId: $field
                value: {
                    singleSelectOptionId: $value
                }
            }
        ) { projectV2Item { id } }
    }' -Variables @{ project = $Project; item = $Item; field = $Field; value = $Value } | Out-Null
}

Function Get-GraphQlProjectNumberGivenName($Organization, $Name) {
    $projectNumber = ""
    $cursor = ""
    While ([String]::IsNullOrEmpty($projectNumber)) {
        $o = Invoke-GitHubGraphQlApi -Query '
        query($organization: String!, $after: String) {
            organization (login: $organization) {
                projectsV2 (first: 20, after: $after) {
                    nodes { id title number }
                    pageInfo { hasNextPage endCursor }
                }
            }
        }' -Variables @{ organization = $Organization; after = $cursor }

        If($false -eq $o.organization.projectsV2.pageInfo.hasNextPage) {
            Break
        }

        $pl = $o.organization.projectsV2.nodes | Where-Object title -Like $Name
        If($null -ne $pl) {
            $projectNumber = $pl.number
            Break
        }

        $cursor = $o.organization.projectsV2.pageInfo.endCursor
    }
    $projectNumber
}

Function Get-GraphQlProjectWithNodes($Organization, $Number) {
    # It's terrible, but it pulls *all* of the info we need all at once!
    $cursor = ""
    $hasMore = $true
    $nodes = @()
    While ($hasMore) {
        $Project = Invoke-GitHubGraphQlApi -Query '
        query($organization: String!, $number: Int!, $after: String) {
            organization(login: $organization) {
                projectV2(number: $number) {
                    id
                    number
                    title
                    fields(first:20){
                        nodes {
                            ... on ProjectV2FieldCommon { id name }
                            ... on ProjectV2SingleSelectField { options { id name } }
                        }
                    }
                    items(first: 100, after: $after) {
                        pageInfo { hasNextPage endCursor }
                        nodes {
                            id
                            status: fieldValueByName(name: "Status") {
                                ... on ProjectV2ItemFieldSingleSelectValue { name }
                            }
                            content {
                                ... on Issue {
                                    number
                                    closedByPullRequestsReferences(first: 8) {
                                        ... on PullRequestConnection {
                                            nodes {
                                                number
                                                mergeCommit { oid }
                                            }
                                        }
                                    }
                                }
                                ... on PullRequest {
                                    number
                                    mergeCommit { oid }
                                }
                            }
                        }
                    }
                }
            }
        }
        ' -Variables @{ organization = $Organization; number = $Number; after = $cursor }

        $n += $Project.organization.projectV2.items.nodes
        $cursor = $Project.organization.projectV2.items.pageInfo.endCursor
        $hasMore = $Project.organization.projectV2.items.pageInfo.hasNextPage
    }
    $Project.organization.projectV2.items.nodes = $n
    $Project
}

If ([String]::IsNullOrEmpty($Version)) {
    $BranchVersionRegex = [Regex]"^release-(\d+(\.\d+)+)$"
    $Branch = & git rev-parse --abbrev-ref HEAD
    $Version = $BranchVersionRegex.Match($Branch).Groups[1].Value
    If ([String]::IsNullOrEmpty($Version)) {
        Write-Error "No version specified, and we can't infer it from the name of your branch ($Branch)."
        Exit 1
    }
    Write-Host "Inferred servicing version $Version"
}

$Script:TodoStatusName = "To Cherry Pick"
$Script:DoneStatusName = "Cherry Picked"
$Script:RejectStatusName = "Rejected"

# Propagate default values into all PSGitHub cmdlets
$GithubOrg = "microsoft"
$PSDefaultParameterValues['*GitHub*:Owner'] = $GithubOrg
$PSDefaultParameterValues['*GitHub*:RepositoryName'] = "terminal"

& git fetch --all 2>&1 | Out-Null

$Branch = & git rev-parse --abbrev-ref HEAD
$RemoteForCurrentBranch = & git config "branch.$Branch.remote"
$CommitsBehind = [int](& git rev-list --count "$RemoteForCurrentBranch/$Branch" "^$Branch")

If ($LASTEXITCODE -Ne 0) {
    Write-Error "Failed to determine branch divergence"
    Exit 1
}

If ($CommitsBehind -Gt 0) {
    Write-Error "Local branch $Branch is out of date with $RemoteForCurrentBranch; consider git pull"
    Exit 1
}

$projectNumber = Get-GraphQlProjectNumberGivenName -Organization $GithubOrg -Name "$Version Servicing Pipeline"
If([String]::IsNullOrEmpty($projectNumber)) {
    Throw "Failed to find ProjectV2 for `"$Version Servicing Pipeline`""
}

$Project = Get-GraphQlProjectWithNodes -Organization $GithubOrg -Number $projectNumber

$StatusField = $Project.organization.projectV2.fields.nodes | Where-Object { $_.name -eq "Status" }
$StatusFieldId = $StatusField.id
$StatusRejectOptionId = $StatusField.options | Where-Object name -eq $script:RejectStatusName | Select-Object -Expand id
$StatusDoneOptionId = $StatusField.options | Where-Object name -eq $script:DoneStatusName | Select-Object -Expand id

$ToPickList = $Project.organization.projectV2.items.nodes | Where-Object { $_.status.name -eq $TodoStatusName }

$commits = New-Object System.Collections.ArrayList
$cards = [System.Collections.Generic.Dictionary[String, String[]]]::new()
$ToPickList | ForEach-Object {
    If (-Not [String]::IsNullOrEmpty($_.content.mergeCommit.oid)) {
        $co = $_.content.mergeCommit.oid
    } ElseIf (-Not [String]::IsNullOrEmpty($_.content.closedByPullRequestsReferences.nodes.mergeCommit.oid)) {
        $co = $_.content.closedByPullRequestsReferences.nodes.mergeCommit.oid
    } Else {
        Return
    }
    $null = $commits.Add($co)
    $cards[$co] += $_.id
}

$sortedAllCommits = & git rev-list --no-walk=sorted $commits
[Array]::Reverse($sortedAllCommits)

$PickArgs = @()
If ($GpgSign) {
    $PickArgs += , "-S"
}

$sortedAllCommits | ForEach-Object {
    Write-Host "`e[96m`e[1;7mPICK`e[22;27m`e[m $(& git show -q --pretty=oneline $_)"
    $null = & git cherry-pick -x $_ 2>&1
    $Err = ""
    While ($True) {
        If ($LASTEXITCODE -ne 0) {
            $Err
            Enter-ConflictResolutionShell

            If ($Global:Abort) {
                & git cherry-pick --abort
                Write-Host -ForegroundColor "Red" "YOU'RE ON YOUR OWN"
                Exit
            }

            If ($Global:Reject) {
                ForEach($card In $cards[$_]) {
                    Set-GraphQlProjectEntryStatus -Project $Project.organization.projectV2.id -Item $card -Field $StatusFieldId -Value $StatusRejectOptionId
                }
                # Fall through to Skip
            }

            If ($Global:Skip) {
                & git cherry-pick --skip
                Break
            }

            $Err = & git cherry-pick --continue --no-edit
        } Else {
            & git commit @PickArgs --amend --no-edit --trailer "Service-Card-Id:$($cards[$_])" --trailer "Service-Version:$Version" | Out-Null
            Write-Host "`e[92;1;7m OK `e[m"
            ForEach($card In $cards[$_]) {
                Set-GraphQlProjectEntryStatus -Project $Project.organization.projectV2.id -Item $card -Field $StatusFieldId -Value $StatusDoneOptionId
            }
            Break
        }
    }
}
