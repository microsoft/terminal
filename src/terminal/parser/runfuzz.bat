mkdir %_NTTREE%\unittests\ft_fuzzer
CALL .\ft_fuzzer\run.bat 10 %_NTTREE%\unittests\ft_fuzzer
for /f %%f in ('dir /B %_NTTREE%\unittests\ft_fuzzer\*.bin') DO (
    CALL .\ft_fuzzwrapper\run.bat %%f 0
    CALL .\ft_fuzzwrapper\run.bat %%f 1200
    CALL .\ft_fuzzwrapper\run.bat %%f 437
)
