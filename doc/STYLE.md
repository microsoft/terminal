# Coding Style

## Philosophy
1. If it's inserting something into the existing classes/functions, try to follow the existing style as closely as possible.
2. If it's brand new code or refactoring a complete class or area of the code, please follow as Modern C++ of a style as you can and reference the [C++ Core Guidelines](https://github.com/isocpp/CppCoreGuidelines) as much as you possibly can.
3. When working with any Win32 or NT API, please try to use the [Windows Implementation Library](./WIL.md) smart pointers and result handlers.
4. The use of NTSTATUS as a result code is discouraged, HRESULT or exceptions are preferred. Functions should not return a status code if they would always return a successful status code. Any function that returns a status code should be marked `noexcept` and have the `nodiscard` attribute.

### Indentation

Code and nested text should use multiples of 4 spaces of indentation, and no
tab characters, like so:

    void func(const int x)
    {
        std::cout << x << std::endl;
    }

### Code Blocks

Please use 4-space indentation to trigger code parsing, rather than [fenced code blocks](https://help.github.com/articles/github-flavored-markdown/#fenced-code-blocks) or any other style, like so:

    This is some document text, with an example below:

        void func()
        {
            std::cout << "This is code." << std::endl;
        }
