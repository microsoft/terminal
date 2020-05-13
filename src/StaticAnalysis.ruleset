<?xml version="1.0" encoding="utf-8"?>
<RuleSet Name="Console Rules" Description="These rules enforce static analysis on console code." ToolsVersion="15.0">

  <Include Path="cppcorecheckrules.ruleset" Action="Error" />

  <Rules AnalyzerId="Microsoft.Analyzers.NativeCodeAnalysis" RuleNamespace="Microsoft.Rules.Native">	
    <Rule Id="C6001" Action="Error" />	
    <Rule Id="C6011" Action="Error" />	
    <!-- We can't do dynamic cast because RTTI is off. -->
    <!-- RTTI is off because Windows OS policies believe RTTI has too much binary size impact for the value and is less portable than RTTI-off modules. -->
    <Rule Id="C26466" Action="None" />
    <!-- This one has caught us off guard as it suddenly showed up. Re-enablement is going to be in #2941 -->
    <Rule Id="C26814" Action="None" />
  </Rules>
  

</RuleSet>
