rem @echo off

rem Batch file to build and upload files
rem 
rem TODO: Integration with FL

set name=meSpeak

rem To upload, this var must be set here or in other batch
rem set ftp=ftp://<user>:<password>@<ftp>/<path>

echo Building %name% ...

msdev ..\eSpeak.dsp /MAKE "eSpeak - Win32 Release" /REBUILD
msdev ..\eSpeak.dsp /MAKE "eSpeak - Win32 Unicode Release" /REBUILD

echo Generating files for %name% ...

del *.zip
del *.dll
copy ..\Docs\%name%_changelog.txt
copy ..\Docs\%name%_version.txt
copy ..\Docs\%name%_readme.txt
mkdir Plugins
cd Plugins
del /Q *.*
copy ..\..\..\..\bin\release\Plugins\%name%.dll
cd ..
mkdir Docs
cd Docs
del /Q *.*
copy ..\..\Docs\%name%_readme.txt
rem copy ..\..\Docs\langpack_%name%.txt
rem copy ..\..\Docs\helppack_%name%.txt
copy ..\..\m_speak.h
cd ..
mkdir Dictionaries
cd Dictionaries
mkdir Voice
cd Voice
xcopy ..\..\..\espeak-data\*.* /S
cd ..
cd ..
mkdir Icons
cd Icons
copy ..\..\..\spellchecker\Flags\flags.dll
cd ..
mkdir src
cd src
del /Q *.*
copy ..\..\*.h
copy ..\..\*.c*
copy ..\..\*.
copy ..\..\*.rc
copy ..\..\*.dsp
copy ..\..\*.dsw
mkdir Docs
cd Docs
del /Q *.*
copy ..\..\..\Docs\*.*
cd ..
mkdir sdk
cd sdk
del /Q *.*
copy ..\..\..\sdk\*.*
cd ..
mkdir lib
cd lib
del /Q *.*
copy ..\..\..\lib\*.*
cd ..
mkdir res
cd res
del /Q *.*
copy ..\..\..\res\*.*
cd ..
mkdir eSpeak
cd eSpeak
del /Q *.*
copy ..\..\..\eSpeak\*.*
cd ..
cd ..
copy ..\Release\%name%.pdb
copy ..\Unicode_Release\%name%W.pdb


"C:\Program Files\Filzip\Filzip.exe" -a -rp %name%.zip Plugins Docs Icons Dictionaries

cd Plugins
del /Q *.*
copy ..\..\..\..\bin\release\Plugins\%name%W.dll
cd ..

"C:\Program Files\Filzip\Filzip.exe" -a -rp %name%W.zip Plugins Docs Icons Dictionaries

"C:\Program Files\Filzip\Filzip.exe" -a -rp %name%_src.zip src\*.*
"C:\Program Files\Filzip\Filzip.exe" -a -rp %name%.pdb.zip %name%.pdb
"C:\Program Files\Filzip\Filzip.exe" -a -rp %name%W.pdb.zip %name%W.pdb

del *.pdb
rd /S /Q Plugins
rd /S /Q Docs
rd /S /Q Dictionaries
rd /S /Q src
rd /S /Q Icons

if "%ftp%"=="" GOTO END

echo Going to upload files...
pause

"C:\Program Files\FileZilla\FileZilla.exe" -u .\%name%.zip %ftp% -overwrite -close 
"C:\Program Files\FileZilla\FileZilla.exe" -u .\%name%W.zip %ftp% -overwrite -close 
"C:\Program Files\FileZilla\FileZilla.exe" -u .\%name%.pdb.zip %ftp% -overwrite -close 
"C:\Program Files\FileZilla\FileZilla.exe" -u .\%name%W.pdb.zip %ftp% -overwrite -close 
"C:\Program Files\FileZilla\FileZilla.exe" -u .\%name%_changelog.txt %ftp% -overwrite -close 
"C:\Program Files\FileZilla\FileZilla.exe" -u .\%name%_version.txt %ftp% -overwrite -close 
"C:\Program Files\FileZilla\FileZilla.exe" -u .\%name%_readme.txt %ftp% -overwrite -close 

if "%ftp2%"=="" GOTO END

"C:\Program Files\FileZilla\FileZilla.exe" -u .\%name%.zip %ftp2% -overwrite -close 
"C:\Program Files\FileZilla\FileZilla.exe" -u .\%name%W.zip %ftp2% -overwrite -close 
"C:\Program Files\FileZilla\FileZilla.exe" -u .\%name%.pdb.zip %ftp2% -overwrite -close 
"C:\Program Files\FileZilla\FileZilla.exe" -u .\%name%W.pdb.zip %ftp2% -overwrite -close 
"C:\Program Files\FileZilla\FileZilla.exe" -u .\%name%_changelog.txt %ftp2% -overwrite -close 
"C:\Program Files\FileZilla\FileZilla.exe" -u .\%name%_version.txt %ftp2% -overwrite -close 
"C:\Program Files\FileZilla\FileZilla.exe" -u .\%name%_readme.txt %ftp2% -overwrite -close 

:END

echo Done.
