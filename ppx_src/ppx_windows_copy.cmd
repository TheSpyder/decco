@REM assume this is run from the parent folder

set "folder="
for /D %%b in ("ppx_src\_esy\default\store\b\*.") do (
  set "folder=%%~b"
)
copy "%folder%\default\bin\bin.exe" "ppx-windows.exe"