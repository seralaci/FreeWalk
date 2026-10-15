@echo off
rem Release package: dist\FreeWalk-<version>[-<name>]\ (run build.cmd first)
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\package.ps1" %*
