#!/bin/bash
log_dir=$(pwd)/bin/log/
log_file="$(pwd)/bin/log/ffmpeg_rd.log"
backup_dir="$(pwd)/bin/log/backup/"

# 创建备份目录（如果不存在）
mkdir -p $backup_dir

# 获取当前日期
current_date=$(date +"%Y-%m-%d")

# 将当前日志文件重命名为带日期的备份文件
mv $log_file $backup_dir"ffmpeg-$current_date.log"

# 重新创建新的日志文件
touch $log_file

# 删除7天前的日志
find $backup_dir -name "ffmpeg-*.log" -mtime +7 -exec rm {} \;

