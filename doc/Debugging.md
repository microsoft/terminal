# Debugging Miscellanea

This file contains notes about debugging various items in the repository.

## Setting breakpoints in Visual Studio for Cascadia (packaged) application

If you want to debug code in the Cascadia package via Visual Studio, your breakpoints will not be hit by default. A tweak is required to the *CascadiaPackage* project in order to enable this.

1. Right-click on *CascadiaPackage* in Solution Explorer and select Properties.
2. Change the *Application process* type from *Mixed (Managed and Native)* to *Native Only*.