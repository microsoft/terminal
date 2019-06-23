# Interface of the Profiles.json

```ts
declare module namespace {

    export interface Profiles {
        globals: Globals;
        profiles: Profile[];
        schemes: Scheme[];
    }

    export interface Keybinding {
        command: string;
        keys: string[];
    }

    export interface Globals {
        alwaysShowTabs: boolean;
        defaultProfile: string;
        initialCols: number;
        initialRows: number;
        keybindings: Keybinding[];
        requestedTheme: string;
        showTabsInTitlebar: boolean;
        showTerminalTitleInTitlebar: boolean;
    }

    export class Profile {
        acrylicOpacity: number;
        closeOnExit: boolean;
        colorScheme: string;
        commandline: string;
        cursorColor: string;
        cursorShape: string;
        fontFace: string;
        fontSize: number;
        guid: string;
        historySize: number;
        name: string;
        padding: string;
        snapOnInput: boolean;
        useAcrylic: boolean = false;
        icon: string;
        backgroundImage: string;
        backgroundImageOpacity: number;
        backgroundImageStretchMode: "uniformToFill";
    }

    export interface Scheme {
        background: string;
        black: string;
        blue: string;
        brightBlack: string;
        brightBlue: string;
        brightCyan: string;
        brightGreen: string;
        brightPurple: string;
        brightRed: string;
        brightWhite: string;
        brightYellow: string;
        cyan: string;
        foreground: string;
        green: string;
        name: string;
        purple: string;
        red: string;
        white: string;
        yellow: string;
    }

}
```