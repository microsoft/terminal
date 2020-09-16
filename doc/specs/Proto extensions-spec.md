---
author: <Pankaj> <Bhojwani> <@PankajBhojwani>
created on: <2020-9-9>
last updated: <2020-9-9>
---

# Proto extensions

## Abstract

This spec outlines adding support for proto extensions. This would allow other apps/programs
to add json snippets to our json files, and will be used when we generate settings for our various profiles. 

## Inspiration

### Goal: Allow programs to have a say in the settings for their profiles

Currently, Ubuntu/WSL/Powershell etc are unable to provide any specifications for how they want
their profiles in Terminal to look - only we and the users can modify the profiles. We want to provide
these installations with the functionality to have a say in this, allowing them to specify things like
their icon, their font and so on. However, we want to maintain that the user has final say over all of
these settings. 

## Solution Design

Currently, when we load the settings we perform the following steps (this is a simplified description,
for the full version see the [spec for cascading default + user settings](https://github.com/microsoft/terminal/blob/master/doc/specs/%23754%20-%20Cascading%20Default%20Settings.md)):

1. Generate profiles from the defaults file
2. Generate profiles from the dynamic profile generators
3. Layer the user settings on top of all the profiles created in steps 1 and 2
4. Validate the settings

To allow for installations to add in their snippets of json, I propose the addition of a new step
in between 2 and 3:

1. Generate profiles from the defaults file
2. Generate profiles from the dynamic profile generators
3. **Incorporate the additional provided json stubs** - these stubs could be:
   1. modifications to existing profiles
   2. additions of full profiles
   3. additions of colour schemes
4. Layer the user settings on top of all the profiles created in steps 1 through 3 (this includes the user-defined default settings which they wish to apply to all profiles)
5. Validate the settings

### Specifications of the json stubs

As written above, the json stubs could be modifications to existing profiles, additions to full profiles, or additions of colour schemes.

To allow modifications/additions of several profiles in one file and to allow the addition of several colour schemes in one json file,
these stubs should be put into lists in the json file: one list named ```profiles``` and the other list named ```schemes```. For example:

```js
{
    "profiles": [ modifications and additions of profiles go here ],
    "schemes": [ additions of colour schemes go here ]
}
```

An example of a full json file with these fields filled out is shown at the end of this section. 

#### Modifications to existing profiles

The main thing to note for modification of existing profiles is that this will only be used for modifying the
default profiles (cmd/powershell) or the dynamically generated profiles. 

For modifications to existing profiles, the json stub would need to indicate which profile it wishes to modify.
Note that currently, we generate a GUID for dynamic profiles using the "initial" name of the profile (i.e. before
any user changes are applied). For example, the "initial" name of a WSL profile is the \<name\> argument to
"wsl.exe -d \<name\>", and the "initial" name of a Powershell profile is something like "Powershell (ARM)" - depending
on the version. Thus, the stub creator could simply use the same uuidv5 GUID generator we do on that name to obtain the
GUID. Then, in the stub they provide the GUID can be used to identify which profile to modify.

Naturally, we will have to provide documentation on how they could generate the desired GUID themselves. In that documentation,
we will also provide examples showing how the current GUIDs are generated for clarity's sake. 

We might run into the case where multiple json stubs modify the same profile and so they override each other. For the initial implementation, we
are simply going to apply _all_ the changes. Eventually, we will probably want some sort of hierarchy to determine
an order to which changes are applied.

Here is an example of a json file that modifies an existing profile (specifically the Azure cloud shell profile):

```js
{
    "profiles": [
        {
            "guid": "{b453ae62-4e3d-5e58-b989-0a998ec441b8}",
            "fontSize": 16,
            "fontWeight": "thin"
        }
    ]
}
```

**NOTE**: This will *not* change the way the profile looks in the user's settings file. The Azure cloud shell profile
in their settings file (assuming they have made no changes) will still be as below. 

```js
{
    "guid": "{b453ae62-4e3d-5e58-b989-0a998ec441b8}",
    "hidden": false,
    "name": "Azure Cloud Shell",
    "source": "Windows.Terminal.Azure"
}
```
However, the user is free to make changes to it as usual and those changes will have the 'final say'. 

#### Full profile stubs

Technically, full profile stubs do not need to contain anything (they could just be '\{\}'). However we should
have some qualifying minimum criteria before we accept a stub as a full profile. I suggest that we only create
new profile objects from stubs that contain at least the following

* A name
* A commandline argument

As in the case of the dynamic profile generator, if we create a profile that did not exist before (i.e. does not
exist in the user settings), we need to add the profile to the user settings file and re-save that file.

Furthermore, we will add a source field to profiles created this way (again, similar to what we do for dynamic profiles).
The source field value is dependent on how we obtained this json file, and will be touched on in more detail later in the
spec (see "Creation and location(s) of the json files" - "The source field"). 

Here is an example of a json file that contains a full profile:

```js
{
    "profiles": [
        {
            "guid": "{a821ae62-9d4a-3e34-b989-0a998ec283e6}",
            "name": "Cool Profile",
            "commandline": "powershell.exe",
            "antialiasingMode": "aliased",
            "fontWeight": "bold",
            "scrollbarState": "hidden"
        }
    ]
}
```

When this profile gets added back to the user's settings file, it will look similar to the way we currently add
new dynamic profiles to the user's settings file. Going along with the example above, the corresponding
json stub in the user's settings file will be:

```js
{
    "guid": "{a821ae62-9d4a-3e34-b989-0a998ec283e6}",
    "name": "Cool Profile",
    "hidden": "false",
    "source": "local"
}
```
Again, the user will be able to make changes to it as they do for all other profiles. 

#### Creation of colour schemes

As with full profiles, we will have some qualifying criteria for what we accept as full colour schemes: color schemes
must have
* A name
* A background
* A foreground

This will cause the issue of colour schemes being overridden if there are many creations of a colour scheme with the
same name. Since for now all we have to uniquely identify colour schemes *is* the name, we will just let this be.

Here is an example of a json stub that contains a colour scheme:

```js
{
    "schemes": [
        {
            "name": "Postmodern Tango Light",
            "background": '#61D6D6',
            "foreground": '#E74856'
        }
    ]
}
```

This stub will *not* show up in the users settings file, similar to the way our default colour schemes do not show up.

#### Example of a full json file
This is an example of a json file that combines all the above examples. Thus, this json file modifies the Azure Cloud
Shell profile, creates a new profile called 'Cool Profile' and creates a new colour scheme called 'Postmodern Tango Light'.

```js
{
  "profiles": [
    {
      "guid": "{b453ae62-4e3d-5e58-b989-0a998ec441b8}",
      "fontSize": 16,
      "fontWeight": "thin"
    },
    {
      "guid": "{a821ae62-9d4a-3e34-b989-0a998ec283e6}",
      "name": "Cool Profile",
      "commandline": "powershell.exe",
      "antialiasingMode": "aliased",
      "fontWeight": "bold",
      "scrollbarState": "hidden"
    }
  ],
  "schemes": [
    {
      "name": "Postmodern Tango Light",
      "background": "#61D6D6",
      "foreground": "#E74856"
    }
  ]
}
```

### Creation and location(s) of the json files

#### For apps installed through Microsoft store (or similar)

For apps that are installed through something like the Microsoft Store, they should add their json file(s) to
an app extension. During our profile generation, we will probe the OS for app extensions of this type that it
knows about and obtain the json files or objects. These apps should should use msappx for us to know about
their extensions.

#### For apps installed 'traditionally' and third parties/independent users

For apps that are installed 'traditionally', there are 2 cases. The first is that this installation is for all
the users of the system - in this case, the installer should add their json files to the global folder:

```
C:\ProgramData\Microsoft\Windows\Terminal
```

Note: `C:\ProgramData` is a [known folder](https://docs.microsoft.com/en-us/previous-versions/windows/desktop/legacy/bb776911(v=vs.85))
that apps should be able to access. It will be on us to create the folder `Terminal` in `C:\ProgramData\Microsoft\Windows` for this to happen. 

In the second case, the installation is only for the current user. For this case, the installer should add the
json files to the local folder:

```
C:\Users\<user>\AppData\Local\Microsoft\Windows\Terminal
```

This is also where independent users will add their own json files for Terminal to generate/modify profiles.

We will look through both folders mentioned above during profile generation. 

#### The source field

Currently, we allow users an easy way to disable/enable profiles that are generated from certain sources. For
example, a user can easily hide all dynamic profiles with the source `"Windows.Terminal.Wsl"` if they wish to.
To retain this functionality, we will add source fields to profiles we create through proto-extensions. 

For full profiles that came from the *global* folder `C:\ProgramData\Microsoft\Windows\Terminal`,
we will give the value `global` to the source field.

For full profiles that came from the *local* folder `C:\Users\<user>\AppData\Local\Microsoft\Windows\Terminal`,
we will give the value `local` to the source field.

For full profiles that came from app extensions, we will give the value `app` to the source field.



## UI/UX Design

This feature will allow other installations a level of control over how their profiles look in Terminal. For example,
if Ubuntu gets a new icon or a new font they can have those changes be reflected in Terminal users' Ubuntu profiles.

Furthermore, this allows users an easy way to share profiles they have created - instead of needing to modify their
settings file directly they could simply download a json file into the folder
`C:\Users\<user>\AppData\Local\Microsoft\Windows\Terminal`.

## Capabilities

### Accessibility

This change should not affect accessibility.

### Security

Opening a profile causes its commandline argument to be automatically run. Thus, if malicious modifications are made
to existing profiles or new profiles with malicious commandline arguments are added, users could be tricked into running
things they do not want to.

### Reliability

This should not affect reliability - most of what its doing is simply layering json which we already do. 

### Compatibility

Again, there should not be any issues here - the user settings can still be layered after this layering for the user
to have the final say. 

### Performance, Power, and Efficiency

Looking through the additional json files could negatively impact startup time. 

## Potential Issues

Cases which would likely be frustrating:

* An installer dumps a _lot_ of json files into the folder which we need to look through.

## Future considerations

How will this affect the settings UI?

## Resources

N/A
