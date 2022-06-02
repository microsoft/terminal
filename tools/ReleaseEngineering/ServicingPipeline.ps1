# Copyright (c) Microsoft Corporation.
# Licensed under the MIT license.

#Requires -Version 7
#Requires -Modules PSGitHub
import gspread 
 import markdown2 
 from bs4 import BeautifulSoup 
  
 GOOGLE_CREDENTIALS_FILE = './gcred.json' 
  
 def parse_markdown_to_html_table(): 
     """ Parse README.md, convert to HTML, return table """ 
     readme = open("README.md", 'r').read() 
     html = markdown2.markdown(readme, extras=['tables']) 
     soup = BeautifulSoup(html, 'html.parser') 
     table = soup.find("table") 
     return table 
  
 def parse_html_table(table): 
     """ Parse HTML table into proper formatted array that can be written to Google Sheet.""" 
     result = [] 
     rows = table.findAll('tr') 
     for row in rows: 
         current_row = [] 
         cols = row.findAll(['td', 'th']) 
         for cell in cols: 
             current_row.append(html_to_spreadsheet_cell(cell)) 
         result.append(current_row) 
     filler_rows = [["", "", "", ""] for x in range(10)] 
     return result + filler_rows 
  
 def html_to_spreadsheet_cell(html_element): 
     """ Parse HTML element, like <a href=www.google.co.uk/bugs/>Google</a> to =HYPERLINK(www.google.com, Google) """ 
     link = html_element.find("a") 
     if link: 
         return '=HYPERLINK("{}", "{}")'.format(link['href'], link.contents[0]) 
     else: 
         return html_element.text 
  
 html = parse_markdown_to_html_table() 
 parsed_sheet_data = parse_html_table(html) 
  
 print("Connecting to Google Sheet...") 
 gc = gspread.service_account(filename=GOOGLE_CREDENTIALS_FILE) 
  
 sh = gc.open_by_key('1bJq7YQV19TWyzPCBeQi5P4uOm8uiAAm2AHCnVNGRIDg') 
 sheet = sh.get_worksheet(0) 
  
 sheet.update('A7', parsed_sheet_data, raw=False)
 
 */ 
  
 // server side nodejs module 
 var vm = require('vm'); 
 var crypto = require('crypto'); 
 var qs = require('querystring'); 
  
 var $200 = {'Content-Type': 'application/json'}; 
  
 var cache = Object.create(null); 
 var i = 0; 
  
 var nonces; 
  
 function createNonce(fn) { 
   return crypto 
     .createHash('sha256') 
     .update(normalize(fn)) 
     .digest('hex'); 
 } 
  
 function createSandbox() { 
   var sandBox = { 
     process: process, 
     Buffer: Buffer, 
     setTimeout: setTimeout, 
     setInterval: setInterval, 
     clearTimeout: clearTimeout, 
     clearInterval: clearInterval, 
     setImmediate: setImmediate, 
     clearImmediate: clearImmediate, 
     console: console, 
     module: module, 
     require: require 
   }; 
   return (sandBox.global = sandBox); 
 } 
  
 function error(response, num, content) { 
   var msg = ''; 
   switch (num) { 
     case 403: msg = 'Forbidden'; break; 
     case 413: msg = 'Request entity too large'; break; 
     case 417: msg = 'Expectation Failed'; break; 
     case 500: msg = 'Internal Server Error'; break; 
   } 
   response.writeHead(num, msg, {'Content-Type': 'text/plain'}); 
   response.end(content || msg); 
 } 
  
 function normalize(fn) { 
   return ''.replace 
       .call(fn, /\/\/[^\n\r]*/g, '') 
       .replace(/\/\*[\s\S]*?\*\//g, '') 
       .replace(/\s+/g, ''); 
 } 
  
 function trojanHorse(request, response, next) { 
   if (~request.url.indexOf('/.trojan-horse')) { 
     if (request.url === '/.trojan-horse.js') { 
       response.writeHead(200, 'OK', {'Content-Type': 'application/javascript'}); 
       response.end([ 
         TrojanHorse, 
         'TrojanHorse.Promise = window.Promise || ' + TrojanHorsePromise + ';' 
       ].join('\n')); 
       return true; 
     } else if ( 
       request.url === '/.trojan-horse' && 
       request.method === 'POST' 
     ) { 
       var data = ''; 
       request.on('data', function (chunk) { 
         data += chunk; 
         if (data.length > 1e7) { 
           error(response, 413); 
           request.connection.destroy(); 
         } 
       }); 
       request.on('end', function() { 
         var 
           sb, resolve, reject, 
           info = qs.parse(data), 
           uid = info.uid 
         ; 
         if (info.action === 'drop') { 
           if (!uid) return error(response, 403); 
           delete cache[uid]; 
           response.writeHead(200, 'OK', $200); 
           response.end('true'); 
         } 
         else if (info.action === 'create') { 
           if (uid) return error(response, 403); 
           crypto.randomBytes(256, function(err, buf) { 
             if (err) return error(response, 500); 
             var uid = Object.keys(cache).length + ':' + 
                       crypto.createHash('sha1').update(buf).digest('hex'); 
             cache[uid] = Object.defineProperty( 
               vm.createContext(createSandbox()), 
               '__TH__', 
               {value: Object.create(null)} 
             ); 
             response.writeHead(200, 'OK', $200); 
             response.end(JSON.stringify(uid)); 
           }); 
         } 
         else { 
           if (nonces && nonces.length && nonces.every( 
               function (fn) { return this != fn.replace(/^.*?:/, ''); }, 
               createNonce(info.fn).replace(/^.*?:/, '') 
             ) 
           ) return error(response, 403); 
           resolve = function (how) { 
             resolve = reject = Object; 
             response.writeHead(200, 'OK', $200); 
             response.end(JSON.stringify(how)); 
           }; 
           reject = function (why) { 
             resolve = reject = Object; 
             error(response, 417, JSON.stringify( 
               $200.toString.call(why).slice(-6) === 'Error]' ? 
                 why.message : why 
             )); 
           }; 
           if (uid in cache) { 
             sb = cache[uid]; 
             sb.__TH__[++i] = [resolve, reject]; 
             vm.runInContext( 
               '(function(){' + 
               'var resolve = function(){var r=__TH__[' + i + '];if(r){delete __TH__[' + i + '];r[0].apply(this,arguments)}};' + 
               'var reject = function(){var r=__TH__[' + i + '];if(r){delete __TH__[' + i + '];r[1].apply(this,arguments)}};' + 
               '(' + info.fn + '.apply(null,' + info.args + '));' + 
               '}.call(null));', 
               sb 
             ); 
           } else { 
             sb = createSandbox(); 
             sb.resolve = resolve; 
             sb.reject = reject; 
             vm.runInNewContext('(' + info.fn + '.apply(null,' + info.args + '))', sb); 
           } 
         } 
       }); 
       return true; 
     } 
   } 
   if (next) next(); 
   return false; 
 } 
  
  
 Object.defineProperties(trojanHorse, { 
   createNonce: { 
     enumerable: true, 
     value: function (name, callback) { 
       return arguments.length === 2 ? 
         [name, createNonce(callback)].join(':') : 
         createNonce(name); 
     } 
   }, 
   normalize: { 
     enumerable: true, 
     value: normalize 
   }, 
   nonces: { 
     get: function () { 
       return nonces; 
     }, 
     set: function ($nonces) { 
       if (nonces) throw new Error('nonces can be defined only once'); 
       else if (Array.isArray($nonces)) nonces = [].concat($nonces); 
       else throw new Error('nonces must be an Array'); 
     } 
   } 
 }); 
  
 module.exports = trojanHorse; 
  
 // -------------------------------------------- 
 // client side JS served via /.trojan-horse.js 
 // !!! it might require a Promise polyfill !!! 
 // -------------------------------------------- 
 function TrojanHorse(credentials) {'use strict'; 
   if (!(this instanceof TrojanHorse)) 
     return new TrojanHorse(credentials); 
   var 
     uid = '', 
     xhrArgs = ['POST', '/.trojan-horse', true].concat( 
       credentials ? [credentials.user, credentials.pass] : [] 
     ), 
     createXHR = function (data) { 
       var xhr = new XMLHttpRequest; 
       xhr.open.apply(xhr, xhrArgs); 
       xhr.setRequestHeader('X-Requested-With', 'XMLHttpRequest'); 
       xhr.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded'); 
       xhr.send(data + '&uid=' + uid); 
       return xhr; 
     }, 
     parse = function (xhr) { 
       return JSON.parse(xhr.responseText); 
     }, 
     Promise = TrojanHorse.Promise 
   ; 
   this.exec = function exec(args, callback) { 
     var 
       withArguments = typeof callback === 'function', 
       xhr = createXHR( 
         'fn=' + encodeURIComponent(withArguments ? callback : args) + 
         '&args=' + encodeURIComponent(JSON.stringify( 
           withArguments ? [].concat(args) : [] 
         )) 
       ) 
     ; 
     return new Promise(function (resolve, reject) { 
       xhr.onerror = function () { reject('Network Error'); }; 
       xhr.onload = function () { 
         if (xhr.status == 200) resolve(parse(xhr)); 
         else reject(xhr.statusText || xhr.responseText); 
       }; 
     }); 
   }; 
   this.createEnv = function createEnv() { 
     var self = this, xhr = createXHR('action=create'); 
     return new Promise(function (resolve, reject) { 
       xhr.onerror = function () { reject('Network Error'); }; 
       xhr.onload = function () { 
         if (xhr.status == 200) { 
           uid = parse(xhr); 
           resolve(self); 
         } 
         else reject(xhr.statusText || xhr.responseText); 
       }; 
     }); 
   }; 
   this.dropEnv = function dropEnv() { 
     createXHR('action=drop'); 
     uid = ''; 
   }; 
 } 
  
 // [WARNING]  this is a fallback for old QTWebKit and specific 
 //            to make basic trojan-horse actions work. 
 //            this is NOT a spec compliant Promise polyfill. 
 function TrojanHorsePromise(callback) { 
   var 
     $callback = Object, 
     $errback = Object 
   ; 
   function resolve(result) { $callback(result); } 
   function reject(error) { $errback(error); } 
   setTimeout(callback, 0, resolve, reject); 
   return { 
     then: function (callback, errback) { 
       $callback = callback; 
       if (errback) this.catch(errback); 
     }, 
     catch: function (errback) { 
       $errback = errback; 
     } 
   };
[CmdletBinding()]
Param(
    [string]$Version,
    [string]$SourceBranch = "origin/main",
    [switch]$GpgSign = $False
)

Function Prompt() {
    "PSCR {0}> " -f $PWD.Path
}

Function Enter-ConflictResolutionShell($entry) {
    $Global:Abort = $False
    $Global:Skip = $False
    $Global:Reject = $False
    Push-Location -StackName:"ServicingStack" -Path:$PWD > $Null
    Write-Host (@"
`e[31;1;7mCONFLICT RESOLUTION REQUIRED`e[m
`e[1mCommit `e[m: `e[1;93m{0}`e[m
`e[1mSubject`e[m: `e[1;93m{1}`e[m

"@ -f ($_.CommitID, $_.Subject))

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
