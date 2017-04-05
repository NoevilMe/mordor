@SETLOCAL

@SET SOLUTION=Mordor.sln
@SET CONFIG=Release
@SET BUILD32=1

FOR %%A IN (%*) DO (
    @IF /i "%%A" EQU "debug" SET CONFIG=Debug
    @IF /i "%%A" EQU "coverage" SET CONFIG=Debug
    @IF /i "%%A" EQU "no32" SET BUILD32=0
)

REM VS120 is Visual Studio 2013
SET LaunchVCVars="%VS120COMNTOOLS%..\..\VC\vcvarsall.bat"

@IF NOT EXIST %LaunchVCVars% (
  @ECHO Visual Studio not found!
  EXIT /B 1
)

if "%winclientlib%" == "" (
    REM winclientlib variable is needed to find the third party libs
    REM see thirdPartyPaths-win64.props
    REM Normally it should be set as a global env variable if not set
    REM we default to assumption that it is located in the root of the profile.
    REM See winclientlib git project for details

    SET winclientlib=%USERPROFILE%\winclientlib
)

ECHO Adding winclientlib tools to path
REM This is based on knowledge of where happens to be installed on the build machine
REM see winclientlib git project for details
SET PATH=%winclientlib%\tools;%PATH%

@IF "%BUILD32%" EQU "0" (
    ECHO Skipping 32-bit build
    GOTO :build64
)

SETLOCAL

ECHO Building %SOLUTION% Win32
@SET PLATFORM=Win32
@CALL %LaunchVCVars% x86

MSBuild /maxcpucount -P:configuration=%CONFIG% -P:platform=%PLATFORM% %SOLUTION%
@IF ERRORLEVEL 1 EXIT /B

REM Remove the values for PATH, LIB, etc added by 32-Bit LaunchVCVars so 
REM that they don't interfere with the 64 bit build
ENDLOCAL


:build64

ECHO Building %SOLUTION% x64

@SET PLATFORM=x64
@CALL %LaunchVCVars% AMD64

MSBuild /maxcpucount -P:configuration=%CONFIG% -P:platform=%PLATFORM% %SOLUTION%
@IF ERRORLEVEL 1 EXIT /B

ECHO Copying 64-bit unit tests to package directory

@IF NOT EXIST packages (
  MKDIR packages
)

COPY /Y %PLATFORM%\%CONFIG%\tests.exe packages\tests.exe

REM Copy openssl, zlib dlls needed by tests
COPY /Y %PLATFORM%\%CONFIG%\*.dll packages

