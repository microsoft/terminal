mkdir %_NTTREE%\unittests\ft_fuzzer
call .\ft_fuzzer\run.bat 10 %_NTTREE%\unittests\ft_fuzzer
for /f %%f in ('dir /B %_NTTREE%\unittests\ft_fuzzer\*.bin') DO (
    call .\ft_fuzzwrapper\run.bat %%f 0
    call .\ft_fuzzwrapper\run.bat %%f 1200
    call .\ft_fuzzwrapper\run.bat %%f 437
)
