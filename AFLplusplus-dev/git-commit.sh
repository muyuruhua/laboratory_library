#!/bin/bash
# 最简单的 Git 提交脚本

git add . && git commit -m "提交时间: $(date '+%Y-%m-%d %H:%M:%S')" && git push
