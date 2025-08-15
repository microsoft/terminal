param(
    [Parameter(Mandatory = $True)]
    [string]$Path
)

# As per PseudoReplacementDefs.xml in LocStudio
$mapping = [hashtable]::new()
$mapping['a'[0]] = 'ªàáâãäåāăąǻάαад'
$mapping['A'[0]] = 'ÀÁÂÃÄÅĀĂĄǺΆΑ∆ΔΛАД'
$mapping['b'[0]] = 'вьъ'
$mapping['B'[0]] = 'ΒßβВЪЬБ'
$mapping['c'[0]] = '¢çćĉċčсς'
$mapping['C'[0]] = 'ÇĆĈĊČС€'
$mapping['d'[0]] = 'ðďđδ'
$mapping['D'[0]] = 'ÐĎĐ'
$mapping['e'[0]] = 'èéêëēĕėęěέεеёє℮зэ'
$mapping['E'[0]] = 'ÈÉÊËĒĔĖĘĚΈΕΣЕ∑ЁЄЗЄЭ'
$mapping['f'[0]] = 'ƒ'
$mapping['F'[0]] = '₣'
$mapping['g'[0]] = 'ĝğġģ'
$mapping['G'[0]] = 'ĜĞĠĢ'
$mapping['h'[0]] = 'ĥħнћђ'
$mapping['H'[0]] = 'ĤĦΉΗН'
$mapping['i'[0]] = 'ìíîïĩīĭįίιϊіїΐ'
$mapping['I'[0]] = 'ÌÍÎĨĪĬĮİΊΪІЇ'
$mapping['j'[0]] = 'ĵј'
$mapping['J'[0]] = 'ĴЈ'
$mapping['k'[0]] = 'ķĸκкќ'
$mapping['K'[0]] = 'ĶΚЌК'
$mapping['l'[0]] = 'ĺļľŀłℓ'
$mapping['L'[0]] = '£ĹĻĽĿŁ₤'
$mapping['m'[0]] = 'mм'
$mapping['M'[0]] = 'ΜМ'
$mapping['n'[0]] = 'ийлⁿпπήηńņňŉŋñ'
$mapping['N'[0]] = 'ÑŃŅŇŊΝИЙЛП∏'
$mapping['o'[0]] = 'òóôõöøōŏőοσόоǿθб'
$mapping['O'[0]] = 'ÒÓÔÕÖØŌŎŐǾΌΘΟΦΩОФΩΏ'
$mapping['p'[0]] = 'φρр'
$mapping['P'[0]] = 'ΡР'
$mapping['r'[0]] = 'ŕŗřяѓґгř'
$mapping['R'[0]] = 'ŔŖŘЯΓЃҐГ'
$mapping['s'[0]] = 'śŝşѕš'
$mapping['S'[0]] = 'ŚŜŞЅŠ'
$mapping['t'[0]] = 'ţťŧτт'
$mapping['T'[0]] = 'ŢŤŦΤТ'
$mapping['u'[0]] = 'µùúûüũūŭůűųΰυϋύцμџ'
$mapping['U'[0]] = 'ÙÚÛÜŨŪŬŮŰŲЏЦ'
$mapping['v'[0]] = 'ν'
$mapping['w'[0]] = 'ŵωώшщẁẃẅ'
$mapping['W'[0]] = 'ŴШЩẀẂẄ'
$mapping['x'[0]] = '×хж'
$mapping['X'[0]] = 'ΧχХЖ'
$mapping['y'[0]] = 'ýÿŷγўỳу'
$mapping['Y'[0]] = '¥ÝŶΎΥΫỲЎ'
$mapping['z'[0]] = 'źżž'
$mapping['Z'[0]] = 'ŹŻΖŽ'

$content = [System.Xml.XmlDocument]::new()
$content.PreserveWhitespace = $true
$content.Load($Path)

function GetPseudoLocalization([string]$key, [string]$value, [string]$comment) {
    $placeholders = @{}
    $placeholderChar = 0xE000

    # Iterate through all {Locked=...} comments and replace locked
    # words with placeholders from the Unicode Private Use Area.
    foreach ($m in [regex]::Matches($comment, '\{Locked=?([^}]*)\}')) {
        $locked = $m.Groups[1].Value

        # Skip {Locked} and {Locked=qps-ploc} entries
        if (($locked -eq '') -or $locked.Contains('qps-ploc')) {
            return $value
        }

        $lockedList = $locked -split ','

        foreach ($locked in $lockedList) {
            if ($locked.StartsWith('"') -and $locked.EndsWith('"')) {
                $locked = $locked.Substring(1, $locked.Length - 2)
                $locked = $locked.Replace('\"', '"')
            }

            $placeholder = "$([char]$placeholderChar)"
            $placeholderChar++

            $placeholders[$placeholder] = $locked
            $value = $value.Replace($locked, $placeholder)
        }
    }

    # We can't rely on $key.GetHashCode() to be consistent across different runs,
    # because in the future PowerShell may enable UseRandomizedStringHashAlgorithm.
    $hash = [System.Text.Encoding]::UTF8.GetBytes($key)
    $hash = [System.Security.Cryptography.SHA1]::Create().ComputeHash($hash)
    $hash = [System.BitConverter]::ToInt32($hash)
    $rng = [System.Random]::new($hash)

    $lines = $value -split '\r?\n'
    $lines = $lines | ForEach-Object {
        # Replace all characters with pseudo-localized characters
        $newValue = ''
        foreach ($char in $_.ToCharArray()) {
            if ($m = $mapping[$char]) {
                $newValue += $m[$rng.Next(0, $mapping[$char].Length)]
            }
            else {
                $newValue += $char
            }
        }

        # Replace all placeholders with their original values
        foreach ($kv in $placeholders.GetEnumerator()) {
            $newValue = $newValue.Replace($kv.Key, $kv.Value)
        }

        # Add 40% padding to the end of the string
        $paddingLength = [System.Math]::Round(0.4 * $newValue.Length)
        $padding = ' !!!' * ($paddingLength / 4 + 1)
        $newValue + $padding.Substring(0, $paddingLength)
    }
    $lines = $lines -join "`r`n"
    return $lines
}

if ($path.EndsWith(".resw")) {
    foreach ($entry in $content.SelectNodes('/root/data')) {
        $comment = $entry.SelectSingleNode('comment')?.'#text' ?? ''
        $entry.value = GetPseudoLocalization $entry.name $entry.value $comment
    }
}
elseif ($path.EndsWith(".xml")) {
    foreach ($parent in $content.DocumentElement.SelectNodes('//*[@_locID]')) {
        $locID = $parent.GetAttribute('_locID')
        $comment = $parent.SelectSingleNode('comment()[contains(., "_locComment_text")]')?.Value ?? ''

        foreach ($entry in $parent.SelectNodes('text()')) {
            $value = $entry.Value
            if ($value.Trim().Length -ne 0) {
                $entry.Value = GetPseudoLocalization $locID $value $comment
            }
        }
    }

    # Remove all _locComment_text comments
    foreach ($entry in $content.DocumentElement.SelectNodes('//comment()[contains(., "_locComment_text")]')) {
        $null = $entry.ParentNode.RemoveChild($entry)
    }

    # Remove all _locID attributes
    foreach ($entry in $content.DocumentElement.SelectNodes('//*[@_locID]')) {
        $entry.RemoveAttribute('_locID')
    }
}

return $content
