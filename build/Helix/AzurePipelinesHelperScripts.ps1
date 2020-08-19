function GetAzureDevOpsBaseUri
{
    Param(
        [string]$CollectionUri,
        [string]$TeamProject
    )

    return $CollectionUri + $TeamProject
}

function GetQueryTestRunsUri
{
    Param(
        [string]$CollectionUri,
        [string]$TeamProject,
        [string]$BuildUri,
        [switch]$IncludeRunDetails
    )

    if ($IncludeRunDetails)
    {
        $includeRunDetailsParameter = "&includeRunDetails=true"
    }
    else
    {
        $includeRunDetailsParameter = ""
    }

    $baseUri = GetAzureDevOpsBaseUri -CollectionUri $CollectionUri -TeamProject $TeamProject
    $queryUri = "$baseUri/_apis/test/runs?buildUri=$BuildUri$includeRunDetailsParameter&api-version=5.0"
    return $queryUri
}