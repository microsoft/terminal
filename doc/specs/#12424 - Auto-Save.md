---
author: Carlos Zamora @carlos-zamora
created on: 2025-12-23
last updated: 2025-12-23
issue id: "#12424"
---

  

# Auto-Save

## Abstract
This spec outlines a refactor primarily in the settings model project to enable auto-save. The current behavior and obstacles are outlined in the [[#Current Obstacles]] section. The most relevant behaviors include refreshing settings, querying settings data, and the settings editor UI. Overall, the main fundamental change to the settings model is that settings objects will now hold a reference to their pertinent `Json::Value`. When settings are queried or changed, the settings model objects will refer to and update (if necessary) their pertinent `Json::Value` data. For convenience, a `SettingKey` enum will be used to reference settings externally.

With regards to refreshing settings, a `JsonManager` will be introduced to split some of the existing responsibilities of `CascadiaSettings`, specifically with regards to serialization. `SettingsLoader` will be pulled out into a separate class and file, and some or all functionality may be moved to `JsonManager`. `JsonManager` and `CascadiaSettings` will work together to load and update the JSON appropriately. Since settings model objects now hold a reference to their pertinent `Json::Value`, reloading the JSON should be fairly straightforward and the process or propagating changes to the rest of the app should remain largely unchanged.

With regards to updating the settings editor architecture, the deserialization process will be mainly handled by the `JsonManager` to handle updating the pertinent `Json::Value` objects and writing the full JSON tree to the settings.json file. This will also handle creating a backup settings file and performing any conflict resolution, if necessary. This will be a separate stage of introducing auto-save as it builds on the previously mentioned changes.
## Inspiration
It's pretty standard across apps to automatically save changes to settings. Historically, Terminal started with settings being modifiable exclusively through the JSON file. Over time, a settings model and editor were implemented to allow for a UI to manage settings. Significant changes have occurred in this area including the addition of a view model to propagate changes to the settings model and X-Macros to group and access setting data throughout the codebase.

## Current Obstacles
There are a few main behaviors operating as obstacles for auto-save today:
1. Refreshing Settings
	- Current Behavior
		- `AppLogic::_RegisterSettingsChange()` , as expected, registers a listener for whenever the settings.json changes. When that occurs,  the JSON is serialized into a `CascadiaSettings` settings model object (see `AppLogic::ReloadSettings()`-->`AppLogic::_TryLoadSettings()`-->`CascadiaSettings::LoadAll()`-->`CascadiaSettings(SettingsLoader)`).
		- This settings model object tree is propagated throughout various parts of the `TerminalApp` project (and subsequently the `TerminalControl` project as well) . Specifically, a `SettingsChanged` event is raised and handled by...
			- `AppHost::_HandleSettingsChanged()` for updating the minimize to notification area, auto-hide window, show tabs fullscreen, and some theming behaviors.
			- `WindowEmperor` for updating global hotkeys, the OS notification area, and session persistence
			- `TerminalWindow::UpdateSettings()`-->`TerminalPage::SetSettings()` for updating the rest of the settings and propagating them to the right spot
		- `TerminalPage::_RefreshUIForSettingsReload()` is responsible for updating the settings for each `TerminalControl` and `Tab`.  `TerminalPage` creates a `TerminalSettingsCache` which lazily creates and updates a profile guid to `TerminalSettingsCreateResult` map. In `TerminalPage::_MakeTerminalPane()` and `TerminalPaneContent::UpdateSettings()`, `TerminalSettingsCache::TryLookup()` is called to do just that.
		- `TermControl::UpdateControlSettings()` finally applies the output `TerminalSettingsPair` from the cache to the `TermControl`. 
			- `TermControl::_UpdateSettingsFromUIThread()` and `TermControl::_UpdateAppearanceFromUIThread()` update several settings like background image, padding, scroll bar, etc.
			- `ControlCore::UpdateSettings()` updates several settings like renderer changes and acrylic. The chain continues down to `Terminal::UpdateSettings()`.
		- In all cases, the settings are applied by directly setting the class members.
	- Critiques
		- Upon refresh, the entire settings model object tree is recreated and used to replace the existing one. This is overkill when a single small setting was changed, and WinRT object construction is expensive.
		- In setting the class members directly, some settings that may have changed at runtime (i.e. font size) get reset to the value set in the JSON!
		- We don't actually know where a setting came from. This isn't strictly necessary, but it could be nice to have.
		- We're not able to really iterate through settings. "Iteration" was achieved using x-macros.
2. Querying Settings Data
	- Current Behavior
		- As mentioned above, the JSON is serialized into a `CascadiaSettings` settings model object which is the root of a tree of settings model objects that define how to query, manage, and modify settings.
		- Settings are "inheritable", meaning that when they are queried, the value defined in the current layer is preferred over the parent's. For example, a profile setting for the PowerShell profile is resolved by checking the PowerShell profile configuration, then the profile defaults profile object, then the defaults.json profile defaults profile object, and finally the fallback value. Additional layers may exist for fragment extensions.
		- In the settings model, this is handled via the `IInheritable<T>` interface which is implemented by `GlobalAppSettings`, `ActionMap`, `Profile`, `AppearanceConfig`, and `FontConfig`. This interface implements the functionality to reference parent objects to inherit from, create children objects that inherit from the current layer, and getters/setters for settings.
	- Critiques
		- The JSON is serialized to `Json::Value` objects using jsoncpp, which is then used to construct the settings model objects and actually store the relevant settings across different parts of the settings model tree. A settings model object ends up having no knowledge of the actual JSON relevant to it.
		- The getters/setters for settings are written using several macros, which can be frustrating to debug through.
		- The `IInheritable<T>` infrastructure provides no easy way to query what settings are set at any given layer.
3. Settings Editor Architecture
	- Current Behavior
		- The settings editor operates by creating a deep copy of the settings model objects. Specifically, when the editor is first opened, a clone of the `CascadiaSettings` is created. As the user navigates the UI and makes changes, the changes are propagated through a view model layer to the cloned settings model objects. When the save button is pressed, the cloned settings model object is deserialized and written to the settings.json file, triggering a full refresh of the settings model throughout the terminal (see point 1 above).
	- Critiques:
		- The view model is responsible for propagating changes as needed to the settings model. This can be leveraged to avoid fully replacing the JSON with the cloned settings model. Ideally, we wouldn't even clone the settings model.

Additionally, the team had discussed some ideas during December 2025's bug bash that are relevant to this area:
- Inheritance objects can be stored to an array. That way, rather than referencing a parent object directly, we can use an index to point to the parent and quickly retrieve it.
- `CascadiaSettings` is a very large and complex object separated across CascadiaSettings.cpp and CascadiaSettingsSerialization.cpp.
- Iterating through the settings would be really nice. It would require storing the keys somewhere.
- To support the idea of iterating through settings, the settings model could adopt a design pattern of returning `IInspectable` objects for queries. A string can be used to describe the type so that it could be cast appropriately.
## Solution Design
The obstacles listed above can be resolved with several changes made primarily in the settings model. Specifically, storing and querying the settings data will need to be the first set of changes (obstacle 2 above). Once those changes are made, consumers of the settings model can be updated to leverage the new APIs. Specifically, the architecture for refreshing settings (obstacle 1 above) and the settings editor (obstacle 3 above) will need to be updated to use the updated settings model architecture.

### Settings Model changes
With [MTSMSettings.h](https://github.com/microsoft/terminal/blob/main/src/cascadia/TerminalSettingsModel/MTSMSettings.h), we have the following setting metadata grouped together: stored type, setting name, setting JSON key, and default value. This can be leveraged to abstract different parts of our settings model fairly easily.

There are several changes that can be made concurrently to the settings model:
1. `Json::Value` storage and maintenance
	- We can leverage [JsonUtils.h](https://github.com/microsoft/terminal/blob/main/src/cascadia/TerminalSettingsModel/JsonUtils.h) to read/write to the relevant JSON. In having each settings object store and maintain a `Json::Value`, the (de)serialization process is more straightforward and viewing the stored JSON simplifies debugging significantly as it becomes clear what settings are set at each layer.
2. Use `SettingKey` enum to reference settings
	- Rather than exposing an API for each setting, we can enumerate all settings for a settings model object using `enum SettingKey`. This greatly reduces the redundant code that we have for each setting. The resulting API can be simplified to `GetSetting()`, `SetSetting()`, `HasSetting()`, and `ClearSetting()` with each taking a `SettingKey` as a parameter.
	- Admittedly, this does move a lot of the complexity over to the JSON boundary as we now have to perform type validation when (de)serializing the JSON. However, [JsonUtils.h](https://github.com/microsoft/terminal/blob/main/src/cascadia/TerminalSettingsModel/JsonUtils.h) provides `ConversionTrait` templated structs which are specifically designed to determine the behavior to convert to and from JSON. We can leverage these objects to do all the necessary work!
	- This also allows us to very easily represent which settings are defined in a given layer. A `GetCurrentSettings()` API can return all `SettingKey`s that are defined in a given layer.
3. Store all parents as a global list and refer to them by index
	- Rather than storing pointer references to parent objects, we can simplify this relationship by maintaining a global `_parents` list. Children can store a `_parentIndex` member that simply points to the relevant parent in `_parents`.

These changes do bring attention to the first golden rule:
> **Golden Rule #1**: All setting types need a `ConversionTrait`

Doing so defines strict rules for (de)serialization.

The resulting settings objects will look something like this:
```c++
struct Profile : ProfileT<Profile>
{
	enum SettingKey
	{
		HistorySize,
		SnapOnInput,
		AltGrAliasing,
		AnswerbackMessage,
		//...
		SETTINGS_SIZE// IMPORTANT: This MUST be the last value in this enum. It's an unused placeholder.
	}
	
	// Retrieves the resolved value of a given setting
	IInspectable GetSetting(SettingKey settingKey)
	{
		// get value from current layer, if possible
		const auto jsonKey{ _GetJsonKey(settingKey) };
		const auto converter{ _GetConverter(settingKey) };
		if (_json.isMember(jsonKey) && converter.CanConvert(_json[jsonKey]))
		{
			return winrt::box_value(JsonUtils::GetValue(_json[jsonKey], converter));
		}
		
		// ask parent, if available
		if (_parentIndex)
		{
			return _parents[_parentIndex].GetSetting(settingKey);
		}
		
		// default value
		return _GetDefaultValue(settingKey);
	}
	
	// Checks the current layer for a setting definition
	bool HasSetting(SettingKey settingKey)
	{
		const auto jsonKey{ _jsonKeyMap[settingKey] };
		const auto converter{ _GetConverter(settingKey) };
		return _json.isMember(jsonKey) && converter.CanConvert(_json[jsonKey]);
	}
	
	// Retrieves all setting keys that are defined in the current layer
	IVector<SettingKey> CurrentSettings() const
	{
		std::vector<SettingKey> _currentSettings;
		for (int i=0; i< static_cast<int>(SettingKey::SETTINGS_SIZE); i++)
		{
			const auto setting{ static_cast<SettingKey>(i) };
			if (HasSetting(setting))
			{
				_currentSettings.add(setting);
			}
		}
		return std::single_threaded_vector<SettingKey>(std::move(_currentSettings));
	}
	
	// Updates the internal JSON setting with a new value
	void SetSetting(SettingKey settingKey, IInspectable settingValGeneric)
	{
		const auto jsonKey{ _jsonKeyMap[settingKey] };
		const auto converter{ _GetConverter(settingKey) };
		
		if (const auto settingValStrict{ settingValGeneric.try_as<decltype(converter)::type>() })
		{
			JsonUtils::SetValueForKey(_json, jsonKey, settingValStrict)
			// Trigger JSON serialization process
		}
		else
		{
			// invalid type!
			assert(false);
		}
	}

private:
	static std::vector<Model::Profile> _parents{};
	std::optional<size_t> _parentIndex{};
	Json::Value _json;
	
	til::static_map<SettingKey, std::string_view> _jsonKeyMap{
		{ SettingKey::HistorySize, "historySize" },
		{ SettingKey::SnapOnInput, "snapOnInput" },
		//...
	};
	
	// Implementation details have been omitted here for simplicity.
	template<typename T>
	JsonUtils::ConversionTrait<T> _GetConverter(SettingKey key);
}
```

### Refreshing settings
The process of refreshing settings was covered in [[#Current Obstacles]]. Now that settings model objects can store their relevant JSON, we don't need to completely reconstruct the settings model object tree. Instead, `AppLogic::ReloadSettings()` can trigger a new `ReloadSettings()` call on the existing `CascadiaSettings` object.

`CascadiaSettings` already has a `SettingsLoader` object that is responsible for parsing the JSON and creating a simple `Json::Value` representation of the loaded settings (stored as `JsonSettings`). A lot of this infrastructure can be reused. When a `Json::Value` is determined for a settings model object (i.e. `Profile`), that settings model can trigger a new `ResetSettings(Json::Value)` function which simply sets the underlying `_json`.

Given that one of the critiques is that `CascadiaSettings` is very large and complex, the json (de)serialization and management can be pulled out into its own class and files. Consider a new `JsonManager` class with the following division of responsibilities:
- `CascadiaSettings`
	- maintains ownership over the serialized settings model object tree (i.e. `GlobalAppSettings`, `Profile`, etc.)
	- handles cascading changes (i.e. color scheme name changed, so all profiles referencing the color scheme need to be updated)
	- capability to find matching settings model objects from parsed json:
		- `Profile& FindMatchingProfile(Json::Value)` is specifically what comes to mind here. When the JSON is being loaded by the `JsonManager`, the settings editor and model will already have a `Profile` object that needs to be updated. This function (along with any other similar ones that may be needed) helps find the matching existing settings model object to avoid full reserialization.
- `JsonManager`
	- maintains ownership over the root of the `Json::Value` object tree
	- stores parts of the `Json::Value` tree in the appropriate settings model objects in `CascadiaSettings` (or creates one if this is the first load). Much of this logic is already handled by `SettingsLoader` today.
	- handles writing to the JSON file. This includes several implementation details such as creating backup json files, debouncing deserialization events, and conflict resolution between auto-save and json changes.

Since all setting queries are resolved by checking the underlying JSON, the resolved values are updated at query-time. This means that once the entire settings model object tree is updated with the new underlying `Json::Value`s, we can continue with the rest of the refresh process as normal. Settings checked at the `AppHost`, `TerminalWindow`, and `TerminalPage` layers will query any relevant settings and retrieve the updated value. The components relying on `TerminalSettingsCreateResult` will also retrieve the updated values. Minor changes will be needed here to simply use the new `GetSetting()` accessor instead of the former ones (i.e. `CopyOnSelect()`).

An earlier critique in this section was that in setting the class members directly, some settings that may have changed at runtime get reset to the value set in the JSON. In order to address this, a second golden rule must be established:
> **Golden Rule #2**: Anything that can be set at runtime needs a member variable to track the runtime value override separately.

This will be mostly important for appearance settings as those are the most jarring examples where refreshing settings change the app's state. Any changes made at runtime should be stored to member variables and, ideally, they should be stored as deltas over exact numbers. This will allow for changes to font size to still propagate, but on a different base font size.
### Settings editor changes
The process of editing settings was covered in [[#Current Obstacles]]. Fundamentally, in order to implement an auto-save experience, changes made in the settings editor must be applied to the settings model and propagated to the stored JSON. With the aforementioned changes, the settings model is more tightly coupled to the JSON which reduces the burden of updating the JSON. Additionally, any changes made to the JSON will result in a seamless refresh experience that doesn't modify existing runtime settings.

A deep copy of the settings model objects is no longer needed. Instead, the settings editor can use the view model layer to propagate changes to the settings model. The new `SetSetting()` API will trigger the JSON serialization process. The settings model object will raise a `WriteSettings` event that will propagate up through `CascadiaSettings` to the `JsonManager`.

Key changes to the deserialization process:
- `JsonManager` will debounce bursts of `WriteSettings` events.
- The full `Json::Value` tree will already exist, so deserialization should mainly use a `JsonWriter` to write the object tree to disk.
- The settings will be written to `settings.json.tmp`. When that is done, `settings.json` will be backed up to `settings.json.bak` and `settings.json.tmp` will be renamed to `settings.json`. During this critical section, `JsonManager` (or `AppLogic` depending on where the file change listener is located) will temporarily stop listening to file change events.
- With regards to conflict resolution, the disk will always be considered the source of truth. Any pending auto-save will be cancelled in the event that an edit is detected.
- After a write operation is complete, the file change listener must be enabled again and a refresh of the settings must be initiated. The settings refresh will simply reload settings queries as described in [[#Current Obstacles]] by recreating the `TerminalSettingsCreateResult` map and performing other miscellaneous work at the `TerminalWindow` and `TerminalPage` layers.
## Capabilities
### Accessibility
N/A
### Security
The deserialization process already writes a hash dependent on the last time the file was written to. It's important that this behavior is maintained.
### Reliability
The `ConversionTrait` objects and `JsonManager` will ensure that only valid JSON is written to the settings file. Regardless, a backup `settings.json` file will exist to gracefully handle any unexpected issues with conflict resolution.
### Compatibility
N/A
### Performance, Power, and Efficiency
Since the entire settings model object tree won't be reconstructed on a settings reload, there should be an reduction in memory usage and an improved performance.
## Potential Issues
Some settings that allow runtime overrides may be set across multiple sources. Tab color, for example, can be set by a profile, at runtime, via the commandline, and using `DECAC`. It's important that this behavior is maintained.

Font size changes should also be preserved. However, rather than storing the final font size when it is changed at runtime, a delta should be stored. This way, when a font size is reloaded, the runtime changes are still applied relative to the base font size, rather than replacing it entirely.
## Future considerations
None
## Resources
None