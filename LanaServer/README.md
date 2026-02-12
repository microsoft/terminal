# LanaServer - Auth0 Backend Application

A .NET 8.0 web application with Auth0 authentication and Gravatar avatar support.

## Features

- **Auth0 Authentication**: Secure OpenID Connect authentication using Auth0
- **Gravatar Support**: Automatic avatar generation using Gravatar for users without Auth0 profile pictures
- **In-Memory Database**: Uses Entity Framework Core with in-memory database for local development
- **SQL Server Support**: Configurable SQL Server connection for production
- **Swagger UI**: API documentation and testing interface
- **CORS Enabled**: Cross-origin resource sharing enabled for all origins

## Prerequisites

- .NET 8.0 SDK or later
- Auth0 account and application configured
- Azure CLI (for deployment)
- PowerShell (for deployment script)

## Project Structure

```
LanaServer/
├── LanaServer.csproj    # Project file with NuGet dependencies
├── Program.cs           # Main application with Auth0 integration
├── NuGet.Config        # NuGet package source configuration
└── README.md           # This file
```

## Configuration

### Auth0 Setup

1. Create an Auth0 application at https://auth0.com
2. Configure the following settings in your Auth0 application:
   - **Allowed Callback URLs**: `https://your-app.azurewebsites.net/callback`
   - **Allowed Logout URLs**: `https://your-app.azurewebsites.net/`
   - **Application Type**: Regular Web Application

3. Note your Auth0 credentials:
   - Domain (e.g., `your-tenant.auth0.com`)
   - Client ID
   - Client Secret

### Environment Variables

The application reads the following environment variables:

- `Auth0Domain`: Your Auth0 domain
- `Auth0ClientId`: Your Auth0 client ID
- `Auth0ClientSecret`: Your Auth0 client secret
- `PORT`: HTTP port (default: 8080)
- `ConnectionStrings:DefaultConnection`: SQL Server connection string (optional)

## Local Development

### Build and Run

```bash
cd LanaServer
dotnet restore
dotnet build
dotnet run
```

The application will start on `http://localhost:8080` (or the port specified in the PORT environment variable).

### Set Environment Variables (Local)

```bash
# Linux/macOS
export Auth0Domain="your-domain.auth0.com"
export Auth0ClientId="your-client-id"
export Auth0ClientSecret="your-client-secret"

# Windows PowerShell
$env:Auth0Domain = "your-domain.auth0.com"
$env:Auth0ClientId = "your-client-id"
$env:Auth0ClientSecret = "your-client-secret"
```

## Deployment

### Azure Web App Deployment

Use the provided PowerShell script to deploy to Azure:

```powershell
.\deploy-lana-auth0.ps1
```

The script will:
1. Create/update the project files
2. Build and publish the application
3. Create a deployment ZIP file
4. Configure Auth0 environment variables in Azure
5. Deploy to Azure Web App
6. Display deployment logs

### Manual Deployment

1. **Publish the application**:
   ```bash
   dotnet publish -c Release -o ./publish
   ```

2. **Create a ZIP file** of the `publish` folder

3. **Deploy to Azure**:
   ```bash
   az webapp deploy --resource-group <resource-group> --name <app-name> --src-path <path-to-zip> --type zip
   ```

4. **Configure environment variables**:
   ```bash
   az webapp config appsettings set --name <app-name> --resource-group <resource-group> \
     --settings Auth0Domain=<domain> Auth0ClientId=<client-id> Auth0ClientSecret=<client-secret>
   ```

## API Endpoints

### Public Endpoints

- `GET /` - Home page with login link
- `GET /login` - Initiates Auth0 login flow
- `GET /logout` - Logs out user and redirects to home
- `GET /swagger` - Swagger UI documentation

### Protected Endpoints

- `GET /api/me` - Returns authenticated user information including:
  - Name
  - Email
  - Avatar (from Auth0 or Gravatar)
  - Provider
  - All Auth0 claims (for debugging)

### Example Response from `/api/me`

```json
{
  "message": "Willkommen zurück, John Doe!",
  "email": "john.doe@example.com",
  "avatar": "https://www.gravatar.com/avatar/abc123...?d=identicon",
  "provider": "Auth0",
  "claims": [
    { "type": "email", "value": "john.doe@example.com" },
    { "type": "nickname", "value": "johndoe" },
    ...
  ]
}
```

## Security Considerations

⚠️ **IMPORTANT**: 
- Never commit Auth0 client secrets to source control
- Rotate Auth0 client secrets regularly
- Use Azure Key Vault or similar services for production secrets
- The deployment script contains placeholder credentials that must be replaced

## Database

The application includes a simple chat log database model:

- **ChatContext**: Entity Framework DbContext
- **ChatLog**: Entity with UserMessage and AiResponse fields

By default, it uses an in-memory database. To use SQL Server, set the `ConnectionStrings:DefaultConnection` configuration value.

## Troubleshooting

### Build Errors

If you encounter NuGet restore errors, ensure the `NuGet.Config` file is present in the LanaServer directory pointing to nuget.org.

### Auth0 Redirect Issues

If Auth0 redirects fail:
1. Check that the callback URL is correctly configured in Auth0
2. Verify that HTTPS is used in production (Azure)
3. Check that environment variables are set correctly

### Avatar Not Showing

The application will:
1. First try to use the picture from Auth0 claims
2. If not available, generate a Gravatar URL from the user's email
3. Gravatar will show a default "identicon" for emails without Gravatar accounts

## License

This project is provided as-is for demonstration purposes.
