[CmdLetBinding()]
Param(
    [Parameter(Mandatory = $true)] 
    [string]$TestFile,

    [Parameter(Mandatory = $true)] 
    [string]$OutputProjFile,

    [Parameter(Mandatory = $true)] 
    [string]$JobTestSuiteName,

    [Parameter(Mandatory = $true)] 
    [string]$TaefPath,

    [string]$TaefQuery
)

Class TestCollection
{
    [string]$Name
    [string]$SetupMethodName
    [string]$TeardownMethodName
    [System.Collections.Generic.Dictionary[string, string]]$Properties

    TestCollection()
    {
        if ($this.GetType() -eq [TestCollection])
        {
            throw "This class should never be instantiated directly; it should only be derived from."
        }
    }

    TestCollection([string]$name)
    {
        $this.Init($name)
    }

    hidden Init([string]$name)
    {
        $this.Name = $name
        $this.Properties = @{}
    }
}

Class Test : TestCollection 
{
    Test([string]$name)
    {
        $this.Init($name)
    }
}

Class TestClass : TestCollection
{
    [System.Collections.Generic.List[Test]]$Tests

    TestClass([string]$name)
    {
        $this.Init($name)
        $this.Tests = @{}
    }
}

Class TestModule : TestCollection
{
    [System.Collections.Generic.List[TestClass]]$TestClasses

    TestModule([string]$name)
    {
        $this.Init($name)
        $this.TestClasses = @{}
    }
}

function Parse-TestInfo([string]$taefOutput)
{
    enum LineType
    {
        None
        TestModule
        TestClass
        Test
        Setup
        Teardown
        Property
    }
        
    [string]$testModuleIndentation = "        "
    [string]$testClassIndentation = "            "
    [string]$testIndentation = "                "
    [string]$setupBeginning = "Setup: "
    [string]$teardownBeginning = "Teardown: "
    [string]$propertyBeginning = "Property["

    function Get-LineType([string]$line)
    {
        if ($line.Contains($setupBeginning))
        {
            return [LineType]::Setup;
        }
        elseif ($line.Contains($teardownBeginning))
        {
            return [LineType]::Teardown;
        }
        elseif ($line.Contains($propertyBeginning))
        {
            return [LineType]::Property;
        }
        elseif ($line.StartsWith($testModuleIndentation) -and -not $line.StartsWith("$testModuleIndentation "))
        {
            return [LineType]::TestModule;
        }
        elseif ($line.StartsWith($testClassIndentation) -and -not $line.StartsWith("$testClassIndentation "))
        {
            return [LineType]::TestClass;
        }
        elseif ($line.StartsWith($testIndentation) -and -not $line.StartsWith("$testIndentation "))
        {
            return [LineType]::Test;
        }
        else
        {
            return [LineType]::None;
        }
    }

    [string[]]$lines = $taefOutput.Split(@([Environment]::NewLine, "`n"), [StringSplitOptions]::RemoveEmptyEntries)
    [System.Collections.Generic.List[TestModule]]$testModules = @()

    [TestModule]$currentTestModule = $null
    [TestClass]$currentTestClass = $null
    [Test]$currentTest = $null

    [TestCollection]$lastTestCollection = $null

    foreach ($rawLine in $lines)
    {
        [LineType]$lineType = (Get-LineType $rawLine)

        # We don't need the whitespace around the line anymore, so we'll discard it to make things easier.
        [string]$line = $rawLine.Trim()

        if ($lineType -eq [LineType]::TestModule)
        {
            if ($currentTest -ne $null -and $currentTestClass -ne $null)
            {
                $currentTestClass.Tests.Add($currentTest)
            }

            if ($currentTestClass -ne $null -and $currentTestModule -ne $null)
            {
                $currentTestModule.TestClasses.Add($currentTestClass)
            }

            if ($currentTestModule -ne $null)
            {
                $testModules.Add($currentTestModule)
            }

            $currentTestModule = [TestModule]::new($line)
            $currentTestClass = $null
            $currentTest = $null
            $lastTestCollection = $currentTestModule
        }
        elseif ($lineType -eq [LineType]::TestClass)
        {
            if ($currentTest -ne $null -and $currentTestClass -ne $null)
            {
                $currentTestClass.Tests.Add($currentTest)
            }

            if ($currentTestClass -ne $null -and $currentTestModule -ne $null)
            {
                $currentTestModule.TestClasses.Add($currentTestClass)
            }

            $currentTestClass = [TestClass]::new($line)
            $currentTest = $null
            $lastTestCollection = $currentTestClass
        }
        elseif ($lineType -eq [LineType]::Test)
        {
            if ($currentTest -ne $null -and $currentTestClass -ne $null)
            {
                $currentTestClass.Tests.Add($currentTest)
            }

            $currentTest = [Test]::new($line)
            $lastTestCollection = $currentTest
        }
        elseif ($lineType -eq [LineType]::Setup)
        {
            if ($lastTestCollection -ne $null)
            {
                $lastTestCollection.SetupMethodName = $line.Replace($setupBeginning, "")
            }
        }
        elseif ($lineType -eq [LineType]::Teardown)
        {
            if ($lastTestCollection -ne $null)
            {
                $lastTestCollection.TeardownMethodName = $line.Replace($teardownBeginning, "")
            }
        }
        elseif ($lineType -eq [LineType]::Property)
        {
            if ($lastTestCollection -ne $null)
            {
                foreach ($match in [Regex]::Matches($line, "Property\[(.*)\]\s+=\s+(.*)"))
                {
                    [string]$propertyKey = $match.Groups[1].Value;
                    [string]$propertyValue = $match.Groups[2].Value;
                    $lastTestCollection.Properties.Add($propertyKey, $propertyValue);
                }
            }
        }
    }

    if ($currentTest -ne $null -and $currentTestClass -ne $null)
    {
        $currentTestClass.Tests.Add($currentTest)
    }

    if ($currentTestClass -ne $null -and $currentTestModule -ne $null)
    {
        $currentTestModule.TestClasses.Add($currentTestClass)
    }

    if ($currentTestModule -ne $null)
    {
        $testModules.Add($currentTestModule)
    }

    return $testModules
}

Write-Verbose "TaefQuery = $TaefQuery"

$TaefSelectQuery = ""
$TaefQueryToAppend = ""
if($TaefQuery)
{
    $TaefSelectQuery = "/select:`"$TaefQuery`""
    $TaefQueryToAppend = " and $TaefQuery"
}
Write-Verbose "TaefSelectQuery = $TaefSelectQuery"


$taefExe = "$TaefPath\te.exe"
[string]$taefOutput = & "$taefExe" /listproperties $TaefSelectQuery $TestFile | Out-String

[System.Collections.Generic.List[TestModule]]$testModules = (Parse-TestInfo $taefOutput)

$projFileContent = @"
<Project>
  <ItemGroup>
"@

foreach ($testModule in $testModules)
{
    foreach ($testClass in $testModules.TestClasses)
    {
        Write-Host "Generating Helix work item for test class $($testClass.Name)..."
        [System.Collections.Generic.List[string]]$testSuiteNames = @()
        
        $testSuiteExists = $false
        $suitelessTestExists = $false

        foreach ($tests in $testClass.Tests)
        {
            if ($tests.Properties.ContainsKey("TestSuite"))
            {
                [string]$testSuite = $tests.Properties["TestSuite"]

                if (-not $testSuiteNames.Contains($testSuite))
                {
                    Write-Host "    Found test suite $testSuite. Generating Helix work item for it as well."
                    $testSuiteNames.Add($testSuite)
                }

                $testSuiteExists = $true
            }
            else
            {
                $suitelessTestExists = $true
            }
        }

        if ($suitelessTestExists)
        {
            $projFileContent += @"

    <HelixWorkItem Include="$($testClass.Name)" Condition="'`$(TestSuite)'=='$($JobTestSuiteName)'">
      <Timeout>00:30:00</Timeout>
      <Command>call %HELIX_CORRELATION_PAYLOAD%\runtests.cmd /select:"(@Name='$($testClass.Name).*'$(if ($testSuiteExists) { "and not @TestSuite='*'" }))$($TaefQueryToAppend)"</Command>
    </HelixWorkItem>
"@
        }
        
        foreach ($testSuiteName in $testSuiteNames)
        {
            $projFileContent += @"

    <HelixWorkItem Include="$($testClass.Name)-$testSuiteName" Condition="'`$(TestSuite)'=='$($JobTestSuiteName)'">
      <Timeout>00:30:00</Timeout>
      <Command>call %HELIX_CORRELATION_PAYLOAD%\runtests.cmd /select:"(@Name='$($testClass.Name).*' and @TestSuite='$testSuiteName')$($TaefQueryToAppend)"</Command>
    </HelixWorkItem>
"@
        }
    }
}

$projFileContent += @"

  </ItemGroup>
</Project>
"@

Set-Content $OutputProjFile $projFileContent -NoNewline -Encoding UTF8