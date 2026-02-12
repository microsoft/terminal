# --- KONFIGURATION ---
$Root = "C:\Carpuncle"
$BackendDir = "$Root\LanaServer"
$WebAppName = "carpu86"
$ResGroup   = "Carpuncle"
$ZipFile    = "$Root\lana-deploy.zip"

# --- AUTH0 DATEN (Aus deinem Upload übernommen) ---
# ⚠️ SECURITY WARNING: These are PLACEHOLDER credentials for demonstration purposes only!
# ⚠️ DO NOT commit real secrets to source control!
# ⚠️ In production, use Azure Key Vault, environment variables, or secure parameter input.
$Auth0Domain       = "carpuncle-dev.eu.auth0.com"
$Auth0ClientId     = "xKpeKcEMLbPxIEv7lUZLESAWcc4rSQX9"
$Auth0ClientSecret = "L2V_zb8KGS5Y_ynYV8ovVJteKGkotX_aOn4A0OPNUzfWFJVe7NdaHTSZbDsCBk4V" # ⚠️ BITTE ROTIEREN!

# Sicherstellen, dass wir im Root sind
if (-not (Test-Path $Root)) { New-Item -ItemType Directory -Path $Root -Force }
Set-Location $Root

# --- SCHRITT 1: Projektdatei mit Auth0-Paketen (OpenID Connect) ---
Write-Host "1️⃣ Erstelle LanaServer.csproj neu (Auth0 Support)..." -ForegroundColor Cyan
if (-not (Test-Path $BackendDir)) { New-Item -ItemType Directory -Path $BackendDir -Force }

$CsprojContent = @"
<Project Sdk="Microsoft.NET.Sdk.Web">

  <PropertyGroup>
    <TargetFramework>net8.0</TargetFramework>
    <Nullable>enable</Nullable>
    <ImplicitUsings>enable</ImplicitUsings>
  </PropertyGroup>

  <ItemGroup>
    <PackageReference Include="Microsoft.AspNetCore.Authentication.Cookies" Version="2.2.0" />
    <PackageReference Include="Microsoft.AspNetCore.Authentication.OpenIdConnect" Version="8.0.0" />
    <PackageReference Include="Microsoft.EntityFrameworkCore" Version="8.0.0" />
    <PackageReference Include="Microsoft.EntityFrameworkCore.SqlServer" Version="8.0.0" />
    <PackageReference Include="Microsoft.EntityFrameworkCore.InMemory" Version="8.0.0" />
    <PackageReference Include="Swashbuckle.AspNetCore" Version="6.5.0" />
  </ItemGroup>

</Project>
"@
[System.IO.File]::WriteAllText("$BackendDir\LanaServer.csproj", $CsprojContent)

# --- SCHRITT 2: Program.cs mit Auth0 Logik ---
Write-Host "2️⃣ Schreibe Program.cs (Auth0 Integration)..." -ForegroundColor Cyan

$Code = @'
using Microsoft.EntityFrameworkCore;
using System.ComponentModel.DataAnnotations;
using Microsoft.AspNetCore.Authentication.Cookies;
using Microsoft.AspNetCore.Authentication.OpenIdConnect;
using Microsoft.AspNetCore.Authentication;
using Microsoft.IdentityModel.Protocols.OpenIdConnect;
using System.Security.Claims;
using System.Security.Cryptography;
using System.Text;

var builder = WebApplication.CreateBuilder(args);

// 1. Port Konfiguration (Azure Fix)
var port = Environment.GetEnvironmentVariable("PORT") ?? "8080";
builder.WebHost.UseUrls($"http://*:{port}");

// 2. Datenbank
var connectionString = builder.Configuration.GetConnectionString("DefaultConnection");
if (string.IsNullOrEmpty(connectionString)) {
    builder.Services.AddDbContext<ChatContext>(options => options.UseInMemoryDatabase("LanaLocal"));
} else {
    builder.Services.AddDbContext<ChatContext>(options => options.UseSqlServer(connectionString));
}

// 3. AUTHENTIFIZIERUNG (Auth0)
// Wir holen die Werte aus den Umgebungsvariablen (Azure Settings)
var auth0Domain = Environment.GetEnvironmentVariable("Auth0Domain");
var auth0ClientId = Environment.GetEnvironmentVariable("Auth0ClientId");
var auth0ClientSecret = Environment.GetEnvironmentVariable("Auth0ClientSecret");

// Always add authentication services to prevent middleware errors
if (!string.IsNullOrEmpty(auth0Domain))
{
    builder.Services.AddAuthentication(options =>
    {
        options.DefaultAuthenticateScheme = CookieAuthenticationDefaults.AuthenticationScheme;
        options.DefaultSignInScheme = CookieAuthenticationDefaults.AuthenticationScheme;
        options.DefaultChallengeScheme = CookieAuthenticationDefaults.AuthenticationScheme;
    })
    .AddCookie()
    .AddOpenIdConnect("Auth0", options =>
    {
        options.Authority = $"https://{auth0Domain}";
        options.ClientId = auth0ClientId;
        options.ClientSecret = auth0ClientSecret;
        options.ResponseType = OpenIdConnectResponseType.Code;
        
        // Scopes für Profilbilder und Email
        options.Scope.Clear();
        options.Scope.Add("openid");
        options.Scope.Add("profile");
        options.Scope.Add("email");

        options.CallbackPath = new PathString("/callback");
        options.ClaimsIssuer = "Auth0";
        options.SaveTokens = true;

        // WICHTIG FÜR AZURE: Zwingt HTTPS für die Redirect URI
        options.Events = new OpenIdConnectEvents
        {
            OnRedirectToIdentityProvider = context =>
            {
                context.ProtocolMessage.RedirectUri = context.ProtocolMessage.RedirectUri.Replace("http://", "https://");
                return Task.CompletedTask;
            },
            OnRedirectToIdentityProviderForSignOut = (context) =>
            {
                var logoutUri = $"https://{auth0Domain}/v2/logout?client_id={auth0ClientId}";
                var postLogoutUri = context.Properties.RedirectUri;
                if (!string.IsNullOrEmpty(postLogoutUri))
                {
                    if (postLogoutUri.StartsWith("/"))
                    {
                        var request = context.Request;
                        postLogoutUri = request.Scheme + "://" + request.Host + request.PathBase + postLogoutUri;
                    }
                    logoutUri += $"&returnTo={Uri.EscapeDataString(postLogoutUri)}";
                }
                context.Response.Redirect(logoutUri);
                context.HandleResponse();
                return Task.CompletedTask;
            }
        };
    });
}
else
{
    // Register minimal authentication services for local development
    builder.Services.AddAuthentication(CookieAuthenticationDefaults.AuthenticationScheme)
        .AddCookie();
}

builder.Services.AddEndpointsApiExplorer();
builder.Services.AddSwaggerGen();
builder.Services.AddAuthorization();
// NOTE: CORS is configured to allow all origins for development.
// For production, restrict to specific origins using: .WithOrigins("https://yourdomain.com")
builder.Services.AddCors(o => o.AddPolicy("All", p => p.AllowAnyOrigin().AllowAnyMethod().AllowAnyHeader()));

var app = builder.Build();

try {
    using (var scope = app.Services.CreateScope()) {
        scope.ServiceProvider.GetRequiredService<ChatContext>().Database.EnsureCreated();
    }
} catch { }

app.UseSwagger();
app.UseSwaggerUI();

app.UseAuthentication();
app.UseAuthorization();
app.UseCors("All");

// --- ENDPOINTS ---

app.MapGet("/", (HttpContext context) => {
    if (context.User.Identity?.IsAuthenticated == true)
    {
        return Results.Redirect("/api/me");
    }
    return Results.Content("<h1>Carpuncle Auth0 App</h1><a href=\"/login\">Login mit Auth0</a>", "text/html");
});

// LOGIN Endpoint
app.MapGet("/login", async (HttpContext context) =>
{
    await context.ChallengeAsync("Auth0", new AuthenticationProperties { RedirectUri = "/api/me" });
});

// LOGOUT Endpoint
app.MapGet("/logout", async (HttpContext context) =>
{
    await context.SignOutAsync(CookieAuthenticationDefaults.AuthenticationScheme);
    await context.SignOutAsync("Auth0", new AuthenticationProperties { RedirectUri = "/" });
});

// PROFIL & GRAVATAR Endpoint
app.MapGet("/api/me", (HttpContext context) =>
{
    if (context.User.Identity?.IsAuthenticated != true) return Results.Unauthorized();

    // Versuche Daten aus Auth0 Claims zu lesen
    var name = context.User.Identity.Name ?? context.User.Claims.FirstOrDefault(c => c.Type == "nickname")?.Value ?? "User";
    var email = context.User.Claims.FirstOrDefault(c => c.Type == ClaimTypes.Email)?.Value ?? "";
    var picture = context.User.Claims.FirstOrDefault(c => c.Type == "picture")?.Value;

    // Fallback: Wenn Auth0 kein Bild liefert, generiere Gravatar aus Email
    // NOTE: MD5 is used here for Gravatar's hash format (non-cryptographic use case)
    if (string.IsNullOrEmpty(picture) && !string.IsNullOrEmpty(email))
    {
        using (var md5 = MD5.Create())
        {
            var hash = md5.ComputeHash(Encoding.UTF8.GetBytes(email.Trim().ToLower()));
            var sb = new StringBuilder();
            for (int i = 0; i < hash.Length; i++) sb.Append(hash[i].ToString("x2"));
            picture = $"https://www.gravatar.com/avatar/{sb.ToString()}?d=identicon";
        }
    }

    return Results.Ok(new { 
        Message = $"Willkommen zurück, {name}!", 
        Email = email,
        Avatar = picture,
        Provider = "Auth0",
        Claims = context.User.Claims.Select(c => new { c.Type, c.Value }) // Debug Info
    });
});

app.Run();

public class ChatContext : DbContext {
    public ChatContext(DbContextOptions<ChatContext> options) : base(options) { }
    public DbSet<ChatLog> ChatLogs { get; set; }
}
public class ChatLog {
    [Key] public int Id { get; set; }
    public string UserMessage { get; set; } = string.Empty;
    public string AiResponse { get; set; } = string.Empty;
}
'@
[System.IO.File]::WriteAllText("$BackendDir\Program.cs", $Code)

# --- SCHRITT 3: Bauen ---
Write-Host "3️⃣ Baue Projekt..." -ForegroundColor Cyan
Set-Location $BackendDir
dotnet restore
dotnet publish -c Release -o ./publish

if (-not (Test-Path ".\publish\LanaServer.dll")) {
    Write-Host "❌ Build fehlgeschlagen!" -ForegroundColor Red
    return
}

# --- SCHRITT 4: Zippen & Azure Konfig ---
Write-Host "4️⃣ Konfiguriere Azure & Lade hoch..." -ForegroundColor Cyan
if (Test-Path $ZipFile) { Remove-Item $ZipFile -Force }
Start-Sleep -Seconds 1
Compress-Archive -Path ".\publish\*" -DestinationPath $ZipFile -Force

# Login checken
$acc = az account show 2>$null
if ($null -eq $acc) { az login }

# Auth0 Secrets in Azure setzen
Write-Host "⚙️ Setze Auth0 Secrets in Azure..." -ForegroundColor Yellow
az webapp config appsettings set --name $WebAppName --resource-group $ResGroup --settings Auth0Domain=$Auth0Domain Auth0ClientId=$Auth0ClientId Auth0ClientSecret=$Auth0ClientSecret

# Deployment
Write-Host "🚀 Deploying..." -ForegroundColor Cyan
az webapp deploy --resource-group $ResGroup --name $WebAppName --src-path $ZipFile --type zip

Write-Host "✅ FERTIG!" -ForegroundColor Green
Write-Host "⚠️ WICHTIG: Trage 'https://$WebAppName.azurewebsites.net/callback' bei Auth0 als 'Allowed Callback URL' ein!" -ForegroundColor Red
Write-Host "👉 Teste Login: https://$WebAppName.azurewebsites.net/login" -ForegroundColor Cyan
Start-Sleep -Seconds 5
az webapp log tail --name $WebAppName --resource-group $ResGroup
