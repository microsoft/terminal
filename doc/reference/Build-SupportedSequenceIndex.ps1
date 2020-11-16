#requires -version 6.1

<#
.SYNOPSIS
Scan source code and build a list of supported VT sequences.
.DESCRIPTION
Scan source code and build a list of supported VT sequences.
TODO: add more details
#>
[cmdletbinding(DefaultParameterSetName="stdout")]
param(
    [parameter(ParameterSetName="file", mandatory)]
    [string]$OutFile,
    [parameter(ParameterSetName="file")]
    [switch]$Force, # for overwriting $OutFile if it exists

    [parameter(ParameterSetName="stdout")]
    [parameter(ParameterSetName="file")]
    [switch]$NoLogo, # no logo in summary
    [parameter(ParameterSetName="stdout")]
    [switch]$SummaryOnly, # no markdown generated
    [parameter(ParameterSetName="stdout")]
    [parameter(ParameterSetName="file")]
    [switch]$Quiet, # no summary or logo

    [parameter(ParameterSetName="file")]
    [parameter(ParameterSetName="stdout")]
    [string]$SolutionRoot = "..\..",
    [parameter(ParameterSetName="file")]
    [parameter(ParameterSetName="stdout")]
    [string]$InterfacePath = $(join-path $solutionRoot "src\terminal\adapter\ITermDispatch.hpp"),
    [parameter(ParameterSetName="file")]
    [parameter(ParameterSetName="stdout")]
    [string]$ConsoleAdapterPath = $(join-path $solutionRoot "src\terminal\adapter\adaptDispatch.hpp"),
    [parameter(ParameterSetName="file")]
    [parameter(ParameterSetName="stdout")]
    [string]$TerminalAdapterPath = $(join-path $solutionRoot "src\cascadia\terminalcore\terminalDispatch.hpp")
)

if ($PSCmdlet.ParameterSetName -eq "stdout") {
    Write-Verbose "Emitting markdown to STDOUT"
}

<#
    GLOBALS
#>

[semver]$myVer = "0.6-beta"
$sequences = import-csv ".\master-sequence-list.csv"
$base = @{}
$conhost = @{}
$terminal = @{}
$prefix = "https://vt100.net/docs/vt510-rm/"
$repo = "https://github.com/oising/terminal/tree/master"
$conhostUrl = $ConsoleAdapterPath.TrimStart($SolutionRoot).replace("\", "/")
$terminalUrl = $TerminalAdapterPath.TrimStart($SolutionRoot).replace("\", "/")

function Read-SourceFiles {
    # extract base interface
    $baseScanner = [regex]'(?x)virtual\s\w+\s(?<method>\w+)(?s)[^;]+;(?-s).*?(?<seq>(?<=\/\/\s).+)'

    $baseScanner.Matches(($src = get-content -raw $interfacePath)) | foreach-object {
        $match = $_
        #$line = (($src[0..$_.Index] -join "") -split "`n").Length
        #$decl = $_.groups[0].value
        $_.groups["seq"].value.split(",") | ForEach-Object {
            $SCRIPT:base[$_.trim()] = $match.groups["method"].value
        }
    }

    # match overrides of ITermDispatch
    $scanner = [regex]'(?x)\s+\w+\s(?<method>\w+)(?s)[^;]+override;'

    $scanner.Matches(($src = Get-Content -raw $consoleAdapterPath)) | ForEach-Object {
        $line = (($src[0..$_.Index] -join "") -split "`n").Length
        $SCRIPT:conhost[$_.groups["method"].value] = $line
    }

    $scanner.Matches(($src = Get-Content -raw $terminalAdapterPath)) | ForEach-Object {
        $line = (($src[0..$_.Index] -join "") -split "`n").Length
        #write-verbose $_.groups[0].value
        $SCRIPT:terminal[$_.groups["method"].value] = $line
    }
}

function Get-SequenceIndexMarkdown {
    # "Sequence","Parent","Description","Origin","Heading","Subheading", "ImplementedBy", "ConsoleHost","Terminal"

    $heading = $null
    $subheading = $null
<#
    Emit markdown

    TODO:
    - auto-generate TOC
#>
@"
# VT Function Support

## Table of Contents

* [Code Extension Functions](#code-extension-functions)
    * [Control Coding](#control-coding)
    * [Character Coding](#character-coding)
    * [Graphic Character Sets](#graphic-character-sets)
* [Terminal Management Functions](#terminal-management-functions)
    * [Identification, status, and Initialization](#identification-status-and-initialization)
    * [Emulations](#emulations)
    * [Set-Up](#set-up)
* [Display Coordinate System and Addressing](#display-coordinate-system-and-addressing)
    * [Active Position and Cursor](#active-position-and-cursor)
    * [Margins and Scrolling](#margins-and-scrolling)
    * [Cursor Movement](#cursor-movement)
    * [Horizontal Tabulation](#horizontal-tabulation)
    * [Page Size and Arrangement](#page-size-and-arrangement)
    * [Page Movement](#page-movement)
    * [Status Display](#status-display)
    * [Right to Left](#right-to-left)
* [Window Management](#window-management)
* [Visual Attributes and Renditions](#visual-attributes-and-renditions)
    * [Line Renditions](#line-renditions)
    * [Character Renditions](#character-renditions)
* [Audible Indicators](#audible-indicators)
* [Mode States](#mode-states)
    * [ANSI](#ansi)
    * [DEC Private](#dec-private)
* [Editing Functions](#editing-functions)
* [OLTP Features](#OLTP-features)
    * [Rectangular Area Operations](#rectangular-area-operations)
    * [Data Integrity](#data-integrity)
    * [Macros](#macros)
* [Saving and Restoring Terminal State](#saving-and-restoring-terminal-state)
    * [Cursor Save Buffer](#cursor-save-buffer)
    * [Terminal State Interrogation](#terminal-state-interrogation)
* [Keyboard Processing Functions](#keyboard-processing-functions)
* [Soft Key Mapping (UDK)](#soft-key-mapping-UDK)
* [Soft Fonts (DRCS)](#soft-fonts-drcs)
* [Printing](#printing)
* [Terminal Communication and Synchronization](#terminal-communication-and-synchronization)
* [Text Locator Extension](#text-locator-extension)
* [Session Management Extension](#session-management-extension)
* [Documented Exceptions](#documented-exceptions)

$($sequences | ForEach-Object {
    if ($method = $base[$_.sequence]) {
        $_.ImplementedBy = $method
        $_.ConsoleHost = $conhost[$method]
        $_.Terminal = $terminal[$method]
    }
    # "Sequence","Associated","Description","Origin","Heading","Subheading", "ImplementedBy", "ConsoleHost","Terminal"
    $c0 = "[$($_.Sequence)]($prefix$($_.sequence).html ""View page on vt100.net"")"
    $c1 = "$($_.description)"
    $c2 = "$($_.origin)"
    $c3 = $(if ($_.consolehost) {"[&#x2713;](${repo}/${conhostUrl}#L$($_.consolehost) ""View console host implementation"")"})
    $c4 = $(if ($_.terminal) {"[&#x2713;](${repo}/${terminalUrl}#L$($_.terminal)} ""View windows terminal implementation"")"})

    $shouldRenderHeader = $false

    if ($heading -ne $_.heading) {
        $heading = $_.heading
@"

## $heading

"@
        $shouldRenderHeader = $true
    }

    if ($subheading -ne $_.subheading) {
        $subheading = $_.subheading
@"

### $subheading

"@
        $shouldRenderHeader = $true
    }

    if ($shouldRenderHeader) {
@"

|Symbol|Function|Origin&nbsp;&#x1F5B3;|Console Host|Terminal|
|:-|:--|:--:|:--:|:--:|
"@
    }
@"

|$c0|$c1|$c2|$c3|$c4|
"@
})

---
Generated on $(get-date -DisplayHint DateTime)
"@
}

function Show-Summary {
    write-host "`n$(' '*7)Windows Terminal Sequencer v${myVer}"
    if (-not $NoLogo.IsPresent) {
        Get-Content .\windows-terminal-logo.ans | ForEach-Object { Write-Host $_ }
    }
    $summary = @"
       `e[1mSequence Support:`e[0m

       `e[7m {0:000} `e[0m known in master-sequence-list.csv.
       `e[7m {1:000} `e[0m common members in ITermDispatch base, of which:
       `e[7m {2:000} `e[0m are implemented by ConsoleHost.
       `e[7m {3:000} `e[0m are implemented by Windows Terminal.

"@ -f $sequences.Count, $base.count, $conhost.count, $terminal.Count

    write-host $summary
}

<#
    Entry Point
#>

Read-SourceFiles

if (-not $SummaryOnly.IsPresent) {

    $markdown = Get-SequenceIndexMarkdown

    if ($PSCmdlet.ParameterSetName -eq "file") {
        # send to file and overwrite
        $markdown | Out-File -FilePath $OutFile -Force:$Force.IsPresent -Encoding utf8NoBOM
    } else {
        # send to STDOUT
        $markdown
    }

    if (-not $Quiet.IsPresent) {
        Show-Summary
    }
} else {
    # summary only
    Show-Summary
}
