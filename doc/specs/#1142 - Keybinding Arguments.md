---
author: Mike Griese @zadjii-msft
created on: 2019-06-19
last updated: 2019-07-14
issue id: 1142
---

# Arbitrary Keybindings Arguments

## Abstract

The goal of this change is to both simplify the keybindings, and also enable far
more flexibility when editing a user's keybindings.

Currently, we have many actions that are very similar in implementation - for
example, `newTabProfile0`, `newTabProfile1`, `newTabProfile2`, etc. All these
actions are _fundamentally_ the same function. However, we've needed to define 9
different actions to enable the user to provide different values to the `newTab`
function.

With this change, we'll be able to remove these _essentially_ duplicated events,
and allow the user to specify arbitrary arguments to these functions.

## Inspiration

Largely inspired by the keybindings in VsCode and Sublime Text. Additionally,
much of the content regarding keybinding events being "handled" was designed as
a solution for [#2285].

## Solution Design

We'll need to introduce args to some actions that we already have defined. These
are the actions I'm thinking about when writing this spec:

```csharp
    // These events already exist like this:
    delegate void NewTabWithProfileEventArgs(Int32 profileIndex);
    delegate void SwitchToTabEventArgs(Int32 profileIndex);
    delegate void ResizePaneEventArgs(Direction direction);
    delegate void MoveFocusEventArgs(Direction direction);

    // These events either exist in another form or don't exist.
    delegate void CopyTextEventArgs(Boolean copyWhitespace);
    delegate void ScrollEventArgs(Int32 numLines);
    delegate void SplitProfileEventArgs(Orientation splitOrientation, Int32 profileIndex);
```

Ideally, after this change, the bindings for these actions would look something
like the following:

```js
{ "keys": ["ctrl+shift+1"], "command": "newTabProfile", "args": { "profileIndex":0 } },
{ "keys": ["ctrl+shift+2"], "command": "newTabProfile", "args": { "profileIndex":1 } },
// etc...

{ "keys": ["alt+1"], "command": "switchToTab", "args": { "index":0 } },
{ "keys": ["alt+2"], "command": "switchToTab", "args": { "index":1 } },
// etc...

{ "keys": ["alt+shift+down"], "command": "resizePane", "args": { "direction":"down" } },
{ "keys": ["alt+shift+up"], "command": "resizePane", "args": { "direction":"up" } },
// etc...

{ "keys": ["alt+down"], "command": "moveFocus", "args": { "direction":"down" } },
{ "keys": ["alt+up"], "command": "moveFocus", "args": { "direction":"up" } },
// etc...

{ "keys": ["ctrl+c"], "command": "copy", "args": { "copyWhitespace":true } },
{ "keys": ["ctrl+shift+c"], "command": "copy", "args": { "copyWhitespace":false } },

{ "keys": ["ctrl+shift+down"], "command": "scroll", "args": { "numLines":1 } },
{ "keys": ["ctrl+shift+up"], "command": "scroll", "args": { "numLines":-1 } },

{ "keys": ["ctrl+alt+1"], "command": "splitProfile", "args": { "orientation":"vertical", "profileIndex": 0 } },
{ "keys": ["ctrl+alt+shift+1"], "command": "splitProfile", "args": { "orientation":"horizontal", "profileIndex": 0 } },
{ "keys": ["ctrl+alt+2"], "command": "splitProfile", "args": { "orientation":"vertical", "profileIndex": 1 } },
{ "keys": ["ctrl+alt+shift+2"], "command": "splitProfile", "args": { "orientation":"horizontal", "profileIndex": 1 } },
// etc...
```

Note that instead of having 9 different `newTabProfile<N>` actions, we have a
singular `newTabProfile` action, and that action requires a `profileIndex` in
the `args` object.

Also, pay attention to the last set of keybindings, the `splitProfile` ones.
This is a function that requires two arguments, both an `orientation` and a
`profileIndex`. Before this change we would have needed to create 20 separate
actions (10 profile indices * 2 directions) to handle these cases. Now it can
be done with a single action that can be much more flexible in its
implementation.

### Parsing KeyBinding Arguments

We'll add two new interfaces: `IActionArgs` and `IActionEventArgs`. Classes that
implement `IActionArgs` will contain all the per-action args, like
`CopyWhitespace` or `ProfileIndex`. `IActionArgs` by itself will be an empty
interface, but all other arguments will derive from it. `IActionEventArgs` will
have a single property `Handled`, which will be used for indicating if a
particular event was processed or not. When parsing args, we'll build
`IActionArgs` to contain all the parameters. When dispatching events, we'll
build `IActionEventArgs` using the `IActionArgs` to set all the parameter values.

All current keybinding events will be changed from their current types to
`TypedEventHandler`s. These `TypedEventHandler`s second param will always be an
instance of `IActionEventArgs`. So, for example:

```csharp

delegate void CopyTextEventArgs();
delegate void NewTabEventArgs();
delegate void NewTabWithProfileEventArgs(Int32 profileIndex);
// ...

[default_interface]
runtimeclass AppKeyBindings : Microsoft.Terminal.Settings.IKeyBindings
{
    event CopyTextEventArgs CopyText;
    event NewTabEventArgs NewTab;
    event NewTabWithProfileEventArgs NewTabWithProfile;
```

Becomes:

```csharp
interface IActionArgs { /* Empty */ }

runtimeclass ActionEventArgs
{
    Boolean Handled;
    ActionArgs Args;
}

runtimeclass CopyTextArgs : IActionArgs
{
    Boolean CopyWhitespace;
}

runtimeclass NewTabWithProfileArgs : IActionArgs
{
    Int32 ProfileIndex;
}
runtimeclass NewTabWithProfileEventArgs : NewTabWithProfileArgs, IActionArgs { }

[default_interface]
runtimeclass AppKeyBindings : Microsoft.Terminal.Settings.IKeyBindings
{
    event Windows.Foundation.TypedEventHandler<AppKeyBindings, ActionEventArgs> CopyText;
    event Windows.Foundation.TypedEventHandler<AppKeyBindings, ActionEventArgs> NewTab;
    event Windows.Foundation.TypedEventHandler<AppKeyBindings, ActionEventArgs> NewTabWithProfile;
```

In this above example, the `CopyTextArgs` class actually contains all the
potential arguments to the Copy action. `ActionEventArgs` is the class that
holds any `ActionArgs`. When we parse the arguments, we'll build a
`CopyTextArgs`, and when we're dispatching the event, we'll build a
`ActionEventArgs` that holds a `CopyTextArgs` as its `Args` value, and dispatch
the `ActionEventArgs` object.


We'll also change our existing map in the `AppKeyBindings` implementation.
Currently, it's a `std::unordered_map<KeyChord, ShortcutAction, ...>`, which
uses the `KeyChord` to lookup the `ShortcutAction`. We'll need to introduce a
new type `ActionAndArgs`:

```csharp
runtimeclass ActionAndArgs
{
    ShortcutAction Action;
    IActionArgs Args;
}
```

and we'll change the map in `AppKeyBindings` to a `std::unordered_map<KeyChord,
ActionAndArgs, ...>`.

When we're parsing keybindings, we'll need to construct args for each of the
events to go with each binding. When we find some key chord bound to a given
Action, we'll construct the `IActionArgs` for that action. For many actions,
these args will be an empty class. However, when we do find an action that needs
additional parsing, `AppKeyBindingsSerialization` will do the extra work to
parse the args for that action.

We'll keep a collection of functions that can be used for quickly determining
how to parse the args for an action if necessary. This map will be a
`std::unordered_map<ShortcutAction, function<IActionArgs(Json::Value)>>`. For
most actions which don't require args, the function in this map will be set to
nullptr, and we'll know that the action doesn't need to parse any more args.
However, for actions that _do_ require args, we'll set up a global function that
can be used to parse a json blob into an `IActionArgs`.

Once the `IActionArgs` is built for the keybinding, we'll set it in
`AppKeyBindings` with a updated `AppKeyBindings::SetKeyBinding` call.
`SetKeyBinding`'s signature will be updated to take a `ActionAndArgs` instead.
Should an action not need arguments, the `Args` member can be left `null` in the
`ActionAndArgs`.

### Executing KeyBinding Actions with Arguments

When we're handling a keybinding in `AppKeyBindings::_DoAction`, we'll trigger
the event handlers with the `IActionArgs` we've stored in the map with the
`ShortcutAction`.

Then, in `App`, we'll handle each of these events. We set up lambdas as event
handlers for each event in `App::_HookupKeyBindings`. In each of those
functions, we'll inspect the `IActionArgs` parameter, and use args from its
implementation to call callbacks in the `App` class. We will update `App` to
have methods defined with the actual keybinding function signatures.

Instead of:

```c++
    void App::_HookupKeyBindings(TerminalApp::AppKeyBindings bindings) noexcept
    {
        // ...
        bindings.NewTabWithProfile([this](const auto index) { _OpenNewTab({ index }); });
    }
```

The code will look like:

```c++
    void App::_HookupKeyBindings(TerminalApp::AppKeyBindings bindings) noexcept
    {
        // ...
        bindings.NewTabWithProfile({ this, &App::_OpenNewTab });
    }
    // ...
    void App::_OpenNewTab(const TerminalApp::AppKeyBindings& sender, const NewTabEventArgs& args)
    {
        auto profileIndex = args.ProfileIndex();
        args.Handled(true);
        // ...
    }
```

### Handling Keybinding Events

Common to all implementations of `IActionArgs` is the `Handled` property. This
will let the app indicate if it was able to actually process a keybinding event
or not. While in the large majority of cases, the events will all be marked
handled, there are some scenarios where the Terminal will need to know if the
event could not be performed. For example, in the case of the `copy` event, the
Terminal is only capable of copying text if there's actually a selection active.
If there isn't a selection active, the `App` should make sure to not mark the
event as not handled (it will leave `args.Handled(false)`). The App should only
mark an event handled if it has actually dispatched the event.

When an event is handled, we'll make sure to return `true` from
`AppKeyBindings::TryKeyChord`, so that the terminal does not actually process
that keypress. For events that were not handled by the application, the terminal
will get another chance to dispatch the keypress.

### Serializing KeyBinding Arguments

Similar to how we parse arguments from the json, we'll need to update the
`AppKeyBindingsSerialization` code to be able to serialize the arguments from a
particular `IActionArgs`.

## UI/UX Design

### Keybindings in the New Tab Dropdown

Small modifications will need to be made to the code responsible for the new tab
dropdown. The new tab dropdown currently also displays the keybindings for each
profile in the new tab dropdown. It does this by querying for the keybinding
associated with each action. As we'll be removing the old `ShortcutAction`s that
this dropdown uses, we'll need a new way to find which key chord corresponds to
opening a given profile.

We'll need to be able to not only lookup a keybinding by `ShortcutAction`, but
also by a `ShortcutAction` and `IActionArgs`. We'll need to update the
`AppKeyBindings::GetKeyBinding` method to also accept a `IActionArgs`. We'll
also probably want each `IActionArgs` implementation to define an
`Equals(IActionArgs)` method, so that we can easily check if two different
`IActionArgs` are the same in this method.

## Capabilities
### Accessibility

N/A

### Security

This should not introduce any _new_ security concerns. We're relying on the
security of jsoncpp for parsing json. Adding new keys to the settings file
will rely on jsoncpp's ability to securely parse those json values.

### Reliability

We'll need to make sure that invalid keybindings are ignored. Currently, we
already gracefully ignore keybindings that have invalid `keys` or invalid
`commands`. We'll need to add additional validation on invalid sets of `args`.
When we're parsing the args from a Json blob, we'll make sure to only ever look
for keys we're expecting and ignore everything else.

If a keybinding requires certain args, but those args are not provided, we'll
need to make sure those args each have reasonable default values to use. If for
any reason a reasonable default can't be used for a keybinding argument, then
we'll need to make sure to display an error dialog to the user for that
scenario.

When we're re-serializing settings, we'll only know about the keybinding arg
keys that were successfully parsed. Other keys will be lost on re-serialization.

### Compatibility

This change will need to carefully be crafted to enable upgrading the legacy
keybindings seamlessly. For most actions, the upgrade should be seamless. Since
they already don't have args, their serializations will remain exactly the same.

However, for the following actions that we'll be removing in favor of actions
with arguments, we'll need to leave legacy deserialization in place to be able
to find these old actions, and automatically build the correct `IActionArgs`
for them:

* `newTabProfile<n>`
    - We'll need to make sure to build args with the right `profileIndex`
      corresponding to the old action.
* `switchToTab<n>`
    - We'll need to make sure to build args with the right `index` corresponding
      to the old action.
* `resizePane<direction>` and `moveFocus<direction>`
    - We'll need to make sure to build args with the right `direction`
      corresponding to the old action.
* `scroll<direction>`
    - We'll need to make sure to build args with the right `amount` value
      corresponding to the old action. `Up` will be -1, and `Down` will be 1.

### Performance, Power, and Efficiency

N/A

## Potential Issues

N/A

## Future considerations

* Should we support some sort of conversion from num keys to an automatic arg?
  For example, by default, <kbd>Alt+&lt;N&gt;</kbd> to focuses the
  Nth tab. Currently, those are 8 separate entries in the keybindings. Should we
  enable some way for them be combined into a single binding entry, where the
  binding automatically recieves the number pressed as an arg? I couldn't find
  any prior art of this, so it doesn't seem worth it to try and invent
  currently. This might be something that we want to loop back on, but for the
  time being, it remains out of scope of this PR.
* When we inevitable support extensions, we'll need to allow extensions to also
  be able to support their own custom keybindings and args. We'll probably want
  to pass the settings to the extension to have the extension parse its own
  settings. We'll want to be able to ask the extension for its own set of
  `ActionAndArgs`<sup>[1]</sup> that it builds from the `keybindings`.  Once we
  have that set of actions, we'll be able to store them locally, and dispatch
  them quickly.
  - [1] We probably won't be able to use the `ActionAndArgs` class directly,
    since that class is specific to the actions we define. We'll need another
    way for extensions to be able to uniquely identify their own actions.

## Resources

N/A

[#2285]: https://github.com/microsoft/terminal/issues/2285
