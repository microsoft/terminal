function MakeVersion ( $major, $minor, $datetimeStamp )
{
    $revision, $branch = $datetimeStamp.Split("-")

    if ( $branch -eq $null )
    {
        $branch = ""
    }

    return [PSCustomObject] @{
        Major = $major
        Minor = $minor
        Revision = $revision
        Branch = $branch
    }
}

function MakeVersionFromString ( $str )
{
    $parts = $str.Split(".")
    return MakeVersion ( [int]::Parse($parts[0]) ) ( [int]::Parse($parts[1]) ) $parts[2]
}

function FormatVersion ( $version )
{
    $branch = ""

    if ( $version.Branch -ne "" )
    {
        $branch = "-{0}" -f $version.Branch
    }

    return "{0}.{1}.{2}{3}" -f $version.Major, $version.Minor, $version.Revision, $branch
}

function CompareReleases ( $version1, $version2 )
{
    $cmpMajor = [Math]::Sign($version1.Major - $version2.Major)

    if ( $cmpMajor -ne 0 )
    {
        return $cmpMajor
    }

    return [Math]::Sign($version1.Minor - $version2.Minor)
}

function CompareRevisions ( $version1, $version2 )
{
    return [Math]::Sign($version1.Revision - $version2.Revision)
}

function CompareBranches ( $version1, $version2 )
{
    return $version1.Branch -eq $version2.Branch
}

function GetDatetimeStamp ( $pgoBranch )
{
    $forkSHA = $( git merge-base origin/$pgoBranch HEAD )

    if ( $LastExitCode -ne 0 )
    {
        throw "FAILED: git merge-base"
    }

    $forkDate = ( Get-Date -Date $( git log -1 $forkSHA --date=iso --pretty=format:"%ad" ) ).ToUniversalTime().ToString("yyMMddHHmm")

    if ( $LastExitCode -ne 0 )
    {
        throw "FAILED: Get forkDate"
    }

    return $forkDate + "-" + $pgoBranch.Replace("/", "_").Replace("-", "_").Replace(".", "_")
}