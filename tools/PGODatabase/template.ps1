function Replace-Many ( $string, $dictionary )
{
    foreach ( $key in $dictionary.Keys )
    {
        $field = '$' + $key.ToString()
        $string = $string.Replace( $field, $dictionary[$key].ToString() )
    }

    return $string
}

function FillOut-Template ( $inputPath, $outputPath, $dictionary )
{
    $replaced = Replace-Many ( Get-Content $inputPath ) $dictionary
    Write-Output $replaced | Set-Content $outputPath -Force | Out-Null
}