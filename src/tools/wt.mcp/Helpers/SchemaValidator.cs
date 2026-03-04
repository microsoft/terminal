using System.Text.Json;
using System.Text.Json.Nodes;

/// <summary>
/// Validates Windows Terminal fragment JSON structure.
/// </summary>
internal static class SchemaValidator
{
    /// <summary>
    /// Validates a fragment JSON string. Fragments have a subset structure
    /// (profiles, schemes, actions) so we do structural validation.
    /// </summary>
    public static List<string> ValidateFragment(string json)
    {
        var errors = new List<string>();

        JsonNode? doc;
        try
        {
            doc = JsonNode.Parse(json);
        }
        catch (JsonException ex)
        {
            errors.Add($"Invalid JSON: {ex.Message}");
            return errors;
        }

        if (doc is not JsonObject root)
        {
            errors.Add("Fragment must be a JSON object.");
            return errors;
        }

        // The product silently ignores unknown keys, so flag these as warnings
        // rather than hard errors (still included in the returned list)
        var knownKeys = new HashSet<string> { "profiles", "schemes", "actions" };
        foreach (var key in root.Select(p => p.Key))
        {
            if (!knownKeys.Contains(key))
            {
                errors.Add($"Warning: unknown top-level key \"{key}\" will be ignored. Fragments support: profiles, schemes, actions.");
            }
        }

        // Validate profiles structure
        // The product accepts "profiles" as either a direct array or an object
        // with a "list" key (same as settings.json)
        if (root["profiles"] is JsonNode profilesNode)
        {
            JsonArray? profileArray = null;
            if (profilesNode is JsonArray directArray)
            {
                profileArray = directArray;
            }
            else if (profilesNode is JsonObject profilesObj && profilesObj["list"] is JsonArray listArray)
            {
                profileArray = listArray;
            }
            else
            {
                errors.Add("\"profiles\" must be an array or an object with a \"list\" array.");
            }

            if (profileArray is not null)
            {
                for (var i = 0; i < profileArray.Count; i++)
                {
                    var profile = profileArray[i];
                    if (profile is not JsonObject profileObj)
                    {
                        errors.Add($"profiles[{i}]: expected an object.");
                        continue;
                    }

                    var hasUpdates = profileObj.ContainsKey("updates");
                    var hasGuid = profileObj.ContainsKey("guid");

                    if (!hasUpdates && !hasGuid)
                    {
                        errors.Add($"profiles[{i}]: must have either \"guid\" (new profile) or \"updates\" (modify existing).");
                    }
                }
            }
        }

        // Validate schemes structure
        if (root["schemes"] is JsonNode schemesNode)
        {
            if (schemesNode is JsonArray schemeArray)
            {
                for (var i = 0; i < schemeArray.Count; i++)
                {
                    var scheme = schemeArray[i];
                    if (scheme is not JsonObject schemeObj)
                    {
                        errors.Add($"schemes[{i}]: expected an object.");
                        continue;
                    }

                    if (!schemeObj.ContainsKey("name"))
                    {
                        errors.Add($"schemes[{i}]: missing required \"name\" property.");
                    }
                }
            }
            else
            {
                errors.Add("\"schemes\" must be an array.");
            }
        }

        // Validate actions structure
        if (root["actions"] is JsonNode actionsNode)
        {
            if (actionsNode is JsonArray actionArray)
            {
                for (var i = 0; i < actionArray.Count; i++)
                {
                    var action = actionArray[i];
                    if (action is not JsonObject actionObj)
                    {
                        errors.Add($"actions[{i}]: expected an object.");
                        continue;
                    }

                    // Actions need either a command or nested commands
                    if (!actionObj.ContainsKey("command") && !actionObj.ContainsKey("commands"))
                    {
                        errors.Add($"actions[{i}]: must have \"command\" or \"commands\".");
                    }

                    // Fragments cannot define keybindings
                    if (actionObj.ContainsKey("keys"))
                    {
                        errors.Add($"actions[{i}]: fragments cannot define \"keys\" (keybindings). Only actions are supported.");
                    }
                }
            }
            else
            {
                errors.Add("\"actions\" must be an array.");
            }
        }

        return errors;
    }
}
