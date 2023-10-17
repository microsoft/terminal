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

Function Process-ConflictResolution($commitId, $subject, $card) {
    Write-Host "`e[96m`e[1;7mPICK`e[22;27m $commitId: $subject`e[m"
    $commitPRRegex = [Regex]"\(#(\d+)\)$"
    $PR = $commitPRRegex.Match($subject).Groups[1].Value
    $null = & git cherry-pick -x $commitId 2>&1
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
                Move-GithubProjectCard -CardId $card.id -ColumnId $RejectColumn.id
                # Fall through to Skip
            }
            If ($Global:Skip) {
                & git cherry-pick --skip
                Break
            }
            $Err = & git cherry-pick --continue --no-edit
        } Else {
            & git commit @PickArgs --amend --no-edit --trailer "Service-Card-Id:$($card.Id)" --trailer "Service-Version:$Version" | Out-Null
            Write-Host "`e[92;1;7m OK `e[m"
            Move-GithubProjectCard -CardId $card.id -ColumnId $DoneColumn.id
            Break
        }
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

$Script:TodoColumnName = "To Cherry Pick"
$Script:DoneColumnName = "Cherry Picked"
$Script:RejectColumnName = "Rejected"

# Propagate default values into all PSGitHub cmdlets
$PSDefaultParameterValues['*GitHub*:Owner'] = "microsoft"
$PSDefaultParameterValues['*GitHub*:RepositoryName'] = "terminal"

$Project = Get-GithubProject -Name "$Version Servicing Pipeline"
$AllColumns = Get-GithubProjectColumn -ProjectId $Project.id
$ToPickColumn = $AllColumns | ? Name -Eq $script:TodoColumnName
$DoneColumn = $AllColumns | ? Name -Eq $script:DoneColumnName
$RejectColumn = $AllColumns | ? Name -Eq $script:RejectColumnName
$Cards = Get-GithubProjectCard -ColumnId $ToPickColumn.id

& git fetch --all 2>&1 | Out-Null

$Entries = @(& git log $SourceBranch --grep "(#\($($Cards.Number -Join "\|")\))" "--pretty=format:%H%x1C%s"  |
    ConvertFrom-CSV -Delimiter "`u{001C}" -Header CommitID,Subject)

[Array]::Reverse($Entries)

$PickArgs = @()
If ($GpgSign) {
    $PickArgs += , "-S"
}

$CommitPRRegex = [Regex]"\(#(\d+)\)$"
$Entries | ForEach-Object {
    Write-Host "`e[96m`e[1;7mPICK`e[22;27m $($_.CommitID): $($_.Subject)`e[m"
    $PR = $CommitPRRegex.Match($_.Subject).Groups[1].Value
    $Card = $Cards | ? Number -Eq $PR
    $null = & git cherry-pick -x $_.CommitID 2>&1
    $Err = ""
    While ($True) {
        If ($LASTEXITCODE -ne 0) {
            $Err
            Enter-ConflictResolutionShell $_

            If ($Global:Abort) {
                & git cherry-pick --abort
                Write-Host -ForegroundColor "Red" "YOU'RE ON YOUR OWN"
                Exit
            }

            If ($Global:Reject) {
                Move-GithubProjectCard -CardId $Card.id -ColumnId $RejectColumn.id
                # Fall through to Skip
            }

            If ($Global:Skip) {
                & git cherry-pick --skip
                Break
            }

            $Err = & git cherry-pick --continue --no-edit
        } Else {
            & git commit @PickArgs --amend --no-edit --trailer "Service-Card-Id:$($Card.Id)" --trailer "Service-Version:$Version" | Out-Null
            Write-Host "`e[92;1;7m OK `e[m"
            Move-GithubProjectCard -CardId $Card.id -ColumnId $DoneColumn.id
            Break
        }
    }
}
