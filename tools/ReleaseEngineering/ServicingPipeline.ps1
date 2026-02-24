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
                            type
                            title: fieldValueByName(name: "Title") {
                                ... on ProjectV2ItemFieldTextValue { text }
                            }
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

Enum ServicingStatus {
    Unknown
    ToConsider
    ToCherryPick
    CherryPicked
    Validated
    Shipped
    Rejected
}

Enum ServicingItemType {
    Unknown
    DraftIssue
    Issue
    PullRequest
    Redacted
}

Class ServicingCard {
    [String]$Id
    [Int]$Number
    [String]$Title
    [String]$Commit
    [ServicingStatus]$Status
    [ServicingItemType]$Type
    static [ServicingItemType]TypeFromString($name) {
        $v = Switch -Exact ($name) {
            "DRAFT_ISSUE"    { [ServicingItemType]::DraftIssue }
            "ISSUE"          { [ServicingItemType]::Issue }
            "PULL_REQUEST"   { [ServicingItemType]::PullRequest }
            "REDACTED"       { [ServicingItemType]::Redacted }
            Default          { [ServicingItemType]::Unknown }
        }
        Return $v
    }

    static [ServicingStatus]StatusFromString($name) {
        $v = Switch -Exact ($name) {
            "To Consider"    { [ServicingStatus]::ToConsider }
            "To Cherry Pick" { [ServicingStatus]::ToCherryPick }
            "Cherry Picked"  { [ServicingStatus]::CherryPicked }
            "Validated"      { [ServicingStatus]::Validated }
            "Shipped"        { [ServicingStatus]::Shipped }
            "Rejected"       { [ServicingStatus]::Rejected }
            Default          { [ServicingStatus]::Unknown }
        }
        Return $v
    }

    ServicingCard([object]$node) {
        $this.Id = $node.id
        $this.Title = $node.title.text
        If (-Not [String]::IsNullOrEmpty($node.content.mergeCommit.oid)) {
            $this.Commit = $node.content.mergeCommit.oid
        } ElseIf (-Not [String]::IsNullOrEmpty($node.content.closedByPullRequestsReferences.nodes.mergeCommit.oid)) {
            $this.Commit = $node.content.closedByPullRequestsReferences.nodes.mergeCommit.oid
        }
        If (-Not [String]::IsNullOrEmpty($node.content.number)) {
            $this.Number = [Int]$node.content.number
        }
        $this.Status = [ServicingCard]::StatusFromString($node.status.name)
        $this.Type = [ServicingCard]::TypeFromString($node.type)
    }

    [string]Format() {
        $color = Switch -Exact ($this.Status) {
            ([ServicingStatus]::ToConsider)   { "`e[38:5:166m" }
            ([ServicingStatus]::ToCherryPick) { "`e[38:5:28m" }
            ([ServicingStatus]::CherryPicked) { "`e[38:5:20m" }
            ([ServicingStatus]::Validated)    { "`e[38:5:126m" }
            ([ServicingStatus]::Shipped)      { "`e[38:5:92m" }
            ([ServicingStatus]::Rejected)     { "`e[38:5:160m" }
            Default                           { "`e[m" }
        }
        $symbol = Switch -Exact ($this.Type) {
            ([ServicingItemType]::DraftIssue)  { "`u{25cb}" } # white circle
            ([ServicingItemType]::Issue)       { "`u{2b24}" } # black circle
            ([ServicingItemType]::PullRequest) { "`u{25c0}" } # black left triangle
            ([ServicingItemType]::Redacted)    { "`u{25ec}" } # triangle with dot
            Default                            { "`u{2b24}" }
        }
        $localTitle = $this.Title
        If ($this.Number -Gt 0) {
            $localTitle = "[`e]8;;https://github.com/microsoft/terminal/issues/{1}`e\Link`e]8;;`e\] {0} (#{1})" -f ($this.Title, $this.Number)
        }
        Return "{0}{1}`e[m ({2}) {3}" -f ($color, $symbol, $this.Status, $localTitle)
    }
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

# Create ServicingCards out of each node, but filter out the ones with no commits.
$cards = $Project.organization.projectV2.items.nodes | ForEach-Object { [ServicingCard]::new($_) }

$incompleteCards = $cards | Where-Object { [String]::IsNullOrEmpty($_.Commit) -And $_.Status -Eq ([ServicingStatus]::ToCherryPick) }
If ($incompleteCards.Length -Gt 0) {
    Write-Host "Cards to cherry pick are not associated with commits:"
    $incompleteCards | ForEach-Object {
        Write-Host " - $($_.Format())"
    }
    Write-Host ""
}

$considerCards = $cards | Where-Object Status -Eq ([ServicingStatus]::ToConsider)
If ($considerCards.Length -Gt 0) {
    Write-Host "`e[7m CONSIDERATION QUEUE `e[27m"
    $considerCards | ForEach-Object {
        Write-Host " - $($_.Format())"
    }
    Write-Host ""
}

$commitGroups = $cards | Where-Object { -Not [String]::IsNullOrEmpty($_.Commit) } | Group-Object Commit
$commits = New-Object System.Collections.ArrayList
$finalCardsForCommit = [System.Collections.Generic.Dictionary[String, ServicingCard[]]]::new()

$commitGroups | ForEach-Object {
    $statuses = $_.Group | Select-Object -Unique Status
    If ($statuses.Length -Gt 1) {
        Write-Host "Commit $($_.Name) is present in more than one column:"
        $_.Group | ForEach-Object {
            Write-Host " - $($_.Format())"
        }
        Write-Host "`e[1mIt will be ignored.`e[m`n"
    } Else {
        If ($statuses[0].Status -eq ([ServicingStatus]::ToCherryPick)) {
            $null = $commits.Add($_.Name)
            $finalCardsForCommit[$_.Name] = $_.Group
        }
    }
}

If ($commits.Count -Eq 0) {
    Write-Host "Nothing to do."
    Exit
}

$sortedAllCommits = & git rev-list --no-walk=sorted $commits
[Array]::Reverse($sortedAllCommits)

$PickArgs = @()
If ($GpgSign) {
    $PickArgs += , "-S"
}

$sortedAllCommits | ForEach-Object {
    Write-Host "`e[96m`e[1;7mPICK`e[22;27m`e[m $(& git show -q --pretty=oneline $_)"
    ForEach($card In $finalCardsForCommit[$_]) {
        Write-Host $card.Format()
    }
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
                ForEach($card In $finalCardsForCommit[$_]) {
                    Set-GraphQlProjectEntryStatus -Project $Project.organization.projectV2.id -Item $card.Id -Field $StatusFieldId -Value $StatusRejectOptionId
                }
                # Fall through to Skip
            }

            If ($Global:Skip) {
                & git cherry-pick --skip
                Break
            }

            $Err = & git cherry-pick --continue --no-edit
        } Else {
            & git commit @PickArgs --amend --no-edit --trailer "Service-Card-Id:$($finalCardsForCommit[$_].Id)" --trailer "Service-Version:$Version" | Out-Null
            Write-Host "`e[92;1;7m OK `e[m"
            ForEach($card In $finalCardsForCommit[$_]) {
                Set-GraphQlProjectEntryStatus -Project $Project.organization.projectV2.id -Item $card.Id -Field $StatusFieldId -Value $StatusDoneOptionId
            }
            Break
        }
    }
}
