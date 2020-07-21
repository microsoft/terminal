using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Runtime.Serialization;
using System.Runtime.Serialization.Json;
using System.Text;
using System.Xml.Linq;

namespace HelixTestHelpers
{
    public class TestResult
    {
        public TestResult()
        {
            Screenshots = new List<string>();
            RerunResults = new List<TestResult>();
        }

        public string Name { get; set; }
        public string SourceWttFile { get; set; }
        public bool Passed { get; set; }
        public bool CleanupPassed { get; set; }
        public TimeSpan ExecutionTime { get; set; }
        public string Details { get; set; }

        public List<string> Screenshots { get; private set; }
        public List<TestResult> RerunResults { get; private set; }

        // Returns true if the test pass rate is sufficient to avoid being counted as a failure.
        public bool PassedOrUnreliable(int requiredNumberOfPasses)
        {
            if(Passed)
            {
                return true;
            }
            else
            {
                if(RerunResults.Count == 1)
                {
                    return RerunResults[0].Passed;
                }
                else
                {
                    return RerunResults.Where(r => r.Passed).Count() >= requiredNumberOfPasses;
                }
            }
        }
    }
    
    //
    // Azure DevOps doesn't currently provide a way to directly report sub-results for tests that failed at least once
    // that were run multiple times.  To get around that limitation, we'll mark the test as "Skip" since
    // that's the only non-pass/fail result we can return, and will then report the information about the
    // runs in the "reason" category for the skipped test.  In order to save space, we'll make the following
    // optimizations for size:
    //
    //   1. Serialize as JSON, which is more compact than XML;
    //   2. Don't serialize values that we don't need;
    //   3. Store the URL prefix and suffix for the blob storage URL only once instead of
    //      storing every log and screenshot URL in its entirety; and
    //   4. Store a list of unique error messages and then index into that instead of
    //      storing every error message in its entirety.
    //
    // #4 is motivated by the fact that if a test fails multiple times, it probably failed for the same reason
    // each time, in which case we'd just be repeating ourselves if we stored every error message each time.
    //
    // TODO (https://github.com/dotnet/arcade/issues/2773): Once we're able to directly report things in a
    // more granular fashion than just a binary pass/fail result, we should do that.
    //
    [DataContract]  
    internal class JsonSerializableTestResults
    {  
        [DataMember]
        internal string blobPrefix;
        
        [DataMember]
        internal string blobSuffix;
        
        [DataMember]
        internal string[] errors;
        
        [DataMember]
        internal JsonSerializableTestResult[] results;
    }
    
    [DataContract]  
    internal class JsonSerializableTestResult  
    {
        [DataMember]
        internal string outcome;

        [DataMember]
        internal int duration;
        
        [DataMember(EmitDefaultValue = false)]
        internal string log;
        
        [DataMember(EmitDefaultValue = false)]
        internal string[] screenshots;
        
        [DataMember(EmitDefaultValue = false)]
        internal int errorIndex;
    }
    
    public class TestPass
    {
        public TimeSpan TestPassExecutionTime { get; set; }
        public List<TestResult> TestResults { get; set; }
        
        public static TestPass ParseTestWttFile(string fileName, bool cleanupFailuresAreRegressions, bool truncateTestNames)
        {
            using (var stream = File.OpenRead(fileName))
            {
                var doc = XDocument.Load(stream);
                var testResults = new List<TestResult>();
                var testExecutionTimeMap = new Dictionary<string, List<double>>();

                TestResult currentResult = null;
                long frequency = 0;
                long startTime = 0;
                long stopTime = 0;
                bool inTestCleanup = false;

                bool shouldLogToTestDetails = false;

                long testPassStartTime = 0;
                long testPassStopTime = 0;

                Func<XElement, bool> isScopeData = (elt) =>
                {
                    return
                        elt.Element("Data") != null &&
                        elt.Element("Data").Element("WexContext") != null &&
                        (
                            elt.Element("Data").Element("WexContext").Value == "Cleanup" ||
                            elt.Element("Data").Element("WexContext").Value == "TestScope" ||
                            elt.Element("Data").Element("WexContext").Value == "TestScope" ||
                            elt.Element("Data").Element("WexContext").Value == "ClassScope" ||
                            elt.Element("Data").Element("WexContext").Value == "ModuleScope"
                        );
                };

                Func<XElement, bool> isModuleOrClassScopeStart = (elt) =>
                {
                    return
                        elt.Name == "Msg" &&
                        elt.Element("Data") != null &&
                        elt.Element("Data").Element("StartGroup") != null &&
                        elt.Element("Data").Element("WexContext") != null &&
                            (elt.Element("Data").Element("WexContext").Value == "ClassScope" ||
                            elt.Element("Data").Element("WexContext").Value == "ModuleScope");
                };

                Func<XElement, bool> isModuleScopeEnd = (elt) =>
                {
                    return
                        elt.Name == "Msg" &&
                        elt.Element("Data") != null &&
                        elt.Element("Data").Element("EndGroup") != null &&
                        elt.Element("Data").Element("WexContext") != null &&
                        elt.Element("Data").Element("WexContext").Value == "ModuleScope";
                };

                Func<XElement, bool> isClassScopeEnd = (elt) =>
                {
                    return
                        elt.Name == "Msg" &&
                        elt.Element("Data") != null &&
                        elt.Element("Data").Element("EndGroup") != null &&
                        elt.Element("Data").Element("WexContext") != null &&
                        elt.Element("Data").Element("WexContext").Value == "ClassScope";
                };

                int testsExecuting = 0;
                foreach (XElement element in doc.Root.Elements())
                {
                    // Capturing the frequency data to record accurate
                    // timing data.
                    if (element.Name == "RTI")
                    {
                        frequency = Int64.Parse(element.Attribute("Frequency").Value);
                    }

                    // It's possible for a test to launch another test. If that happens, we won't modify the
                    // current result. Instead, we'll continue operating like normal and expect that we get two
                    // EndTests nodes before our next StartTests. We'll check that we've actually got a stop time
                    // before creating a new result. This will result in the two results being squashed
                    // into one result of the outer test that ran the inner one.
                    if (element.Name == "StartTest")
                    {
                        testsExecuting++;
                        if (testsExecuting == 1)
                        {
                            string testName = element.Attribute("Title").Value;
                            
                            if (truncateTestNames)
                            {
                                const string xamlNativePrefix = "Windows::UI::Xaml::Tests::";
                                const string xamlManagedPrefix = "Windows.UI.Xaml.Tests.";
                                if (testName.StartsWith(xamlNativePrefix))
                                {
                                    testName = testName.Substring(xamlNativePrefix.Length);
                                }
                                else if (testName.StartsWith(xamlManagedPrefix))
                                {
                                    testName = testName.Substring(xamlManagedPrefix.Length);
                                }
                            }

                            currentResult = new TestResult() { Name = testName, SourceWttFile = fileName, Passed = true, CleanupPassed = true };
                            testResults.Add(currentResult);
                            startTime = Int64.Parse(element.Descendants("WexTraceInfo").First().Attribute("TimeStamp").Value);
                            inTestCleanup = false;
                            shouldLogToTestDetails = true;
                            stopTime = 0;
                        }
                    }
                    else if (currentResult != null && element.Name == "EndTest")
                    {
                        testsExecuting--;

                        // If any inner test fails, we'll still fail the outer
                        currentResult.Passed &= element.Attribute("Result").Value == "Pass";

                        // Only gather execution data if this is the outer test we ran initially
                        if (testsExecuting == 0)
                        {
                            stopTime = Int64.Parse(element.Descendants("WexTraceInfo").First().Attribute("TimeStamp").Value);
                            if (!testExecutionTimeMap.Keys.Contains(currentResult.Name))
                                testExecutionTimeMap[currentResult.Name] = new List<double>();
                            testExecutionTimeMap[currentResult.Name].Add((double)(stopTime - startTime) / frequency);
                            currentResult.ExecutionTime = TimeSpan.FromSeconds(testExecutionTimeMap[currentResult.Name].Average());

                            startTime = 0;
                            inTestCleanup = true;
                        }
                    }
                    else if (currentResult != null &&
                            (isModuleOrClassScopeStart(element) || isModuleScopeEnd(element) || isClassScopeEnd(element)))
                    {
                        shouldLogToTestDetails = false;
                        inTestCleanup = false;
                    }

                    // Log-appending methods.
                    if (currentResult != null && element.Name == "Error")
                    {
                        if (shouldLogToTestDetails)
                        {
                            currentResult.Details += "\r\n[Error]: " + element.Attribute("UserText").Value;
                            if (element.Attribute("File") != null && element.Attribute("File").Value != "")
                            {
                                currentResult.Details += (" [File " + element.Attribute("File").Value);
                                if (element.Attribute("Line") != null)
                                    currentResult.Details += " Line: " + element.Attribute("Line").Value;
                                currentResult.Details += "]";
                            }
                        }


                        // The test cleanup errors will often come after the test claimed to have
                        // 'passed'. We treat them as errors as well. 
                        if (inTestCleanup)
                        {
                            currentResult.CleanupPassed = false;
                            currentResult.Passed = false;
                            // In stress mode runs, this test will run n times before cleanup is run. If the cleanup
                            // fails, we want to fail every test.
                            if (cleanupFailuresAreRegressions)
                            {
                                foreach (var result in testResults.Where(res => res.Name == currentResult.Name))
                                {
                                    result.Passed = false;
                                    result.CleanupPassed = false;
                                }
                            }
                        }
                    }

                    if (currentResult != null && element.Name == "Warn")
                    {
                        if (shouldLogToTestDetails)
                        {
                            currentResult.Details += "\r\n[Warn]: " + element.Attribute("UserText").Value;
                        }

                        if (element.Attribute("File") != null && element.Attribute("File").Value != "")
                        {
                            currentResult.Details += (" [File " + element.Attribute("File").Value);
                            if (element.Attribute("Line") != null)
                                currentResult.Details += " Line: " + element.Attribute("Line").Value;
                            currentResult.Details += "]";
                        }
                    }

                    if (currentResult != null && element.Name == "Msg")
                    {
                        var dataElement = element.Element("Data");
                        if (dataElement != null)
                        {
                            var supportingInfo = dataElement.Element("SupportingInfo");
                            if (supportingInfo != null)
                            {
                                var screenshots = supportingInfo.Elements("Item")
                                    .Where(item => GetAttributeValue(item, "Name") == "Screenshot")
                                    .Select(item => GetAttributeValue(item, "Value"));

                                foreach(var screenshot in screenshots)
                                {
                                    string fileNameSuffix = string.Empty;
                                    
                                    if (fileName.Contains("_rerun_multiple"))
                                    {
                                        fileNameSuffix = "_rerun_multiple";
                                    }
                                    else if (fileName.Contains("_rerun"))
                                    {
                                        fileNameSuffix = "_rerun";
                                    }
                                    
                                    currentResult.Screenshots.Add(screenshot.Replace(".jpg", fileNameSuffix + ".jpg"));
                                }
                            }
                        }
                    }
                }

                testPassStartTime = Int64.Parse(doc.Root.Descendants("WexTraceInfo").First().Attribute("TimeStamp").Value);
                testPassStopTime = Int64.Parse(doc.Root.Descendants("WexTraceInfo").Last().Attribute("TimeStamp").Value);

                var testPassTime = TimeSpan.FromSeconds((double)(testPassStopTime - testPassStartTime) / frequency);
                
                foreach (TestResult testResult in testResults)
                {
                    if (testResult.Details != null)
                    {
                        testResult.Details = testResult.Details.Trim();
                    }
                }

                var testpass = new TestPass
                {
                    TestPassExecutionTime = testPassTime,
                    TestResults = testResults
                };

                return testpass;
            }
        }
        
        public static TestPass ParseTestWttFileWithReruns(string fileName, string singleRerunFileName, string multipleRerunFileName, bool cleanupFailuresAreRegressions, bool truncateTestNames)
        {
            TestPass testPass = ParseTestWttFile(fileName, cleanupFailuresAreRegressions, truncateTestNames);
            TestPass singleRerunTestPass = File.Exists(singleRerunFileName) ? ParseTestWttFile(singleRerunFileName, cleanupFailuresAreRegressions, truncateTestNames) : null;
            TestPass multipleRerunTestPass = File.Exists(multipleRerunFileName) ? ParseTestWttFile(multipleRerunFileName, cleanupFailuresAreRegressions, truncateTestNames) : null;
            
            List<TestResult> rerunTestResults = new List<TestResult>();

            if (singleRerunTestPass != null)
            {
                rerunTestResults.AddRange(singleRerunTestPass.TestResults);
            }

            if (multipleRerunTestPass != null)
            {
                rerunTestResults.AddRange(multipleRerunTestPass.TestResults);
            }

            // For each failed test result, we'll check to see whether the test passed at least once upon rerun.
            // If so, we'll set PassedOnRerun to true to flag the fact that this is an unreliable test
            // rather than a genuine test failure.
            foreach (TestResult failedTestResult in testPass.TestResults.Where(r => !r.Passed))
            {
                failedTestResult.RerunResults.AddRange(rerunTestResults.Where(r => r.Name == failedTestResult.Name));
            }

            return testPass;
        }

        private static string GetAttributeValue(XElement element, string attributeName)
        {
            if(element.Attribute(attributeName) != null)
            {
                return element.Attribute(attributeName).Value;
            }

            return null;
        }
    }

    public static class FailedTestDetector
    {
        public static void OutputFailedTestQuery(string wttInputPath)
        {
            var testPass = TestPass.ParseTestWttFile(wttInputPath, cleanupFailuresAreRegressions: true, truncateTestNames: false);
            
            List<string> failedTestNames = new List<string>();
            
            foreach (var result in testPass.TestResults)
            {
                if (!result.Passed)
                {
                    failedTestNames.Add(result.Name);
                }
            }
            
            if (failedTestNames.Count > 0)
            {
                string failedTestSelectQuery = "(@Name='";
                
                for (int i = 0; i < failedTestNames.Count; i++)
                {
                    failedTestSelectQuery += failedTestNames[i];
                    
                    if (i < failedTestNames.Count - 1)
                    {
                        failedTestSelectQuery += "' or @Name='";
                    }
                }
                
                failedTestSelectQuery += "')";
            
                Console.WriteLine(failedTestSelectQuery);
            }
            else
            {
                Console.WriteLine("");
            }
        }
    }

    public class TestResultParser
    {
        private string testNamePrefix;
        private string helixResultsContainerUri;
        private string helixResultsContainerRsas;
    
        public TestResultParser(string testNamePrefix, string helixResultsContainerUri, string helixResultsContainerRsas)
        {
            this.testNamePrefix = testNamePrefix;
            this.helixResultsContainerUri = helixResultsContainerUri;
            this.helixResultsContainerRsas = helixResultsContainerRsas;
        }

        public Dictionary<string, string> GetSubResultsJsonByMethodName(string wttInputPath, string wttSingleRerunInputPath, string wttMultipleRerunInputPath)
        {
            Dictionary<string, string> subResultsJsonByMethod = new Dictionary<string, string>();
            TestPass testPass = TestPass.ParseTestWttFileWithReruns(wttInputPath, wttSingleRerunInputPath, wttMultipleRerunInputPath, cleanupFailuresAreRegressions: true, truncateTestNames: false);
            
            foreach (var result in testPass.TestResults)
            {
                var methodName = result.Name.Substring(result.Name.LastIndexOf('.') + 1);

                if (!result.Passed)
                {
                    // If a test failed, we'll have rerun it multiple times.  We'll record the results of each run
                    // formatted as JSON.
                    JsonSerializableTestResults serializableResults = new JsonSerializableTestResults();
                    serializableResults.blobPrefix = helixResultsContainerUri;
                    serializableResults.blobSuffix = helixResultsContainerRsas;

                    List<string> errorList = new List<string>();
                    errorList.Add(result.Details);

                    foreach (TestResult rerunResult in result.RerunResults)
                    {
                        errorList.Add(rerunResult.Details);
                    }

                    serializableResults.errors = errorList.Distinct().Where(s => s != null).ToArray();

                    var reason = new XElement("reason");
                    List<JsonSerializableTestResult> serializableResultList = new List<JsonSerializableTestResult>();
                    serializableResultList.Add(ConvertToSerializableResult(result, serializableResults.errors));

                    foreach (TestResult rerunResult in result.RerunResults)
                    {
                        serializableResultList.Add(ConvertToSerializableResult(rerunResult, serializableResults.errors));
                    }

                    serializableResults.results = serializableResultList.ToArray();

                    using (MemoryStream stream = new MemoryStream())
                    {
                        DataContractJsonSerializer serializer = new DataContractJsonSerializer(typeof(JsonSerializableTestResults));
                        serializer.WriteObject(stream, serializableResults);
                        stream.Position = 0;

                        using (StreamReader streamReader = new StreamReader(stream))
                        {
                            subResultsJsonByMethod.Add(methodName, streamReader.ReadToEnd());
                        }
                    }
                }
            }

            return subResultsJsonByMethod;
        }

        public void ConvertWttLogToXUnitLog(string wttInputPath, string wttSingleRerunInputPath, string wttMultipleRerunInputPath, string xunitOutputPath, int requiredPassRateThreshold)
        {
            TestPass testPass = TestPass.ParseTestWttFileWithReruns(wttInputPath, wttSingleRerunInputPath, wttMultipleRerunInputPath, cleanupFailuresAreRegressions: true, truncateTestNames: false);
            var results = testPass.TestResults;

            int resultCount = results.Count;
            int passedCount = results.Where(r => r.Passed).Count();
            
            // Since we re-run tests on failure, we'll mark every test that failed at least once as "skipped" rather than "failed".
            // If the test failed sufficiently often enough for it to count as a failed test (determined by a property on the
            // Azure DevOps job), we'll later mark it as failed during test results processing.

            int failedCount = results.Where(r => !r.PassedOrUnreliable(requiredPassRateThreshold)).Count();
            int skippedCount = results.Where(r => !r.Passed && r.PassedOrUnreliable(requiredPassRateThreshold)).Count();

            var root = new XElement("assemblies");

            var assembly = new XElement("assembly");
            assembly.SetAttributeValue("name", "MUXControls.Test.dll");
            assembly.SetAttributeValue("test-framework", "TAEF");
            assembly.SetAttributeValue("run-date", DateTime.Now.ToString("yyyy-mm-dd"));

            // This doesn't need to be completely accurate since it's not exposed anywhere.
            // If we need accurate an start time we can probably calculate it from the te.wtl file, but for
            // now this is fine.
            assembly.SetAttributeValue("run-time", (DateTime.Now - testPass.TestPassExecutionTime).ToString("hh:mm:ss"));
            
            assembly.SetAttributeValue("total", resultCount);
            assembly.SetAttributeValue("passed", passedCount);
            assembly.SetAttributeValue("failed", failedCount);
            assembly.SetAttributeValue("skipped", skippedCount);
            
            assembly.SetAttributeValue("time", (int)testPass.TestPassExecutionTime.TotalSeconds);
            assembly.SetAttributeValue("errors", 0);
            root.Add(assembly);

            var collection = new XElement("collection");
            collection.SetAttributeValue("total", resultCount);
            collection.SetAttributeValue("passed", passedCount);
            collection.SetAttributeValue("failed", failedCount);
            collection.SetAttributeValue("skipped", skippedCount);
            collection.SetAttributeValue("name", "Test collection");
            collection.SetAttributeValue("time", (int)testPass.TestPassExecutionTime.TotalSeconds);
            assembly.Add(collection);

            foreach (var result in results)
            {
                var test = new XElement("test");
                test.SetAttributeValue("name", testNamePrefix + "." + result.Name);

                var className = result.Name.Substring(0, result.Name.LastIndexOf('.'));
                var methodName = result.Name.Substring(result.Name.LastIndexOf('.') + 1);
                test.SetAttributeValue("type", className);
                test.SetAttributeValue("method", methodName);

                test.SetAttributeValue("time", result.ExecutionTime.TotalSeconds);
                
                string resultString = string.Empty;
                
                if (result.Passed)
                {
                    resultString = "Pass";
                }
                else if(result.PassedOrUnreliable(requiredPassRateThreshold))
                {
                    resultString = "Skip";
                }
                else
                {
                    resultString = "Fail";
                }

                
                test.SetAttributeValue("result", resultString);

                if (!result.Passed)
                {
                    // If a test failed, we'll have rerun it multiple times.
                    // We'll save the subresults to a JSON text file that we'll upload to the helix results container -
                    // this allows it to be as long as we want, whereas the reason field in Azure DevOps has a 4000 character limit.
                    string subResultsFileName = methodName + "_subresults.json";
                    string subResultsFilePath = Path.Combine(Path.GetDirectoryName(wttInputPath), subResultsFileName);
					
                    if (result.PassedOrUnreliable(requiredPassRateThreshold))
                    {
                        var reason = new XElement("reason");
                        reason.Add(new XCData(GetUploadedFileUrl(subResultsFileName, helixResultsContainerUri, helixResultsContainerRsas)));
                        test.Add(reason);
                    }
                    else
                    {
                        var failure = new XElement("failure");
                        var message = new XElement("message");
						message.Add(new XCData(GetUploadedFileUrl(subResultsFileName, helixResultsContainerUri, helixResultsContainerRsas)));
                        failure.Add(message);
                        test.Add(failure);
                    }
                }
                collection.Add(test);
            }

            File.WriteAllText(xunitOutputPath, root.ToString());
        }
        
        private JsonSerializableTestResult ConvertToSerializableResult(TestResult rerunResult, string[] uniqueErrors)
        {
            var serializableResult = new JsonSerializableTestResult();
            
            serializableResult.outcome = rerunResult.Passed ? "Passed" : "Failed";
            serializableResult.duration = (int)Math.Round(rerunResult.ExecutionTime.TotalMilliseconds);
            
            if (!rerunResult.Passed)
            {
                serializableResult.log = Path.GetFileName(rerunResult.SourceWttFile);
                
                if (rerunResult.Screenshots.Any())
                {
                    List<string> screenshots = new List<string>();
                    
                    foreach (var screenshot in rerunResult.Screenshots)
                    {
                        screenshots.Add(Path.GetFileName(screenshot));
                    }
                    
                    serializableResult.screenshots = screenshots.ToArray();
                }
                
                // To conserve space, we'll log the index of the error to index in a list of unique errors rather than
                // jotting down every single error in its entirety. We'll add one to the result so we can avoid
                // serializing this property when it has the default value of 0.
                serializableResult.errorIndex = Array.IndexOf(uniqueErrors, rerunResult.Details) + 1;
            }
            
            return serializableResult;
        }

        private string GetUploadedFileUrl(string filePath, string helixResultsContainerUri, string helixResultsContainerRsas)
        {
            var filename = Path.GetFileName(filePath);
            return string.Format("{0}/{1}{2}", helixResultsContainerUri, filename, helixResultsContainerRsas);
        }
    }
}
