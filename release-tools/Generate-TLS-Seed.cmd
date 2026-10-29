@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Generate-TLS-Seed.ps1" %*
if errorlevel 1 (
  echo Seed generation failed. Existing files were not intentionally replaced.
  pause
  exit /b 1
)
pause
