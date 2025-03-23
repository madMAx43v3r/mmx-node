@echo off

.\build.cmd && ^
git add -A dist/ && ^
git commit -m "UI: build"