call rmdir /S /Q openssl32
call mkdir openssl32
call mkdir openssl32\lib
call mkdir openssl32\include
call mkdir openssl32\include\openssl
call rmdir /S /Q openssl64
call mkdir openssl64
call mkdir openssl64\lib
call mkdir openssl64\include
call mkdir openssl64\include\openssl


call del /f /q openssl-1.0.0a\tmp32\*.*
call del /f /q openssl-1.0.0a\out32\*.*
call "\Program Files (x86)\Microsoft Visual Studio 8\vc\bin\vcvars32.bat"
cd openssl-1.0.0a
call ms\do_nasm
call perl Configure VC-WIN32
call ms\do_ms
call nmake -f ms\nt.mak
cd ..
call copy openssl-1.0.0a\inc32\openssl\*.* openssl32\include\openssl
call copy openssl-1.0.0a\out32\libeay32.lib openssl32\lib
call copy openssl-1.0.0a\out32\ssleay32.lib openssl32\lib


call del /f /q openssl-1.0.0a\tmp32\*.*
call del /f /q openssl-1.0.0a\out32\*.*
call "\Program Files (x86)\Microsoft Visual Studio 8\vc\bin\amd64\vcvarsamd64.bat"
cd openssl-1.0.0a
call ms\do_nasm
call perl Configure VC-WIN64A
call ms\do_win64a
call nmake -f ms\nt.mak
cd ..
call copy openssl-1.0.0a\inc32\openssl\*.* openssl64\include\openssl
call copy openssl-1.0.0a\out32\libeay32.lib openssl64\lib
call copy openssl-1.0.0a\out32\ssleay32.lib openssl64\lib

