#!/bin/bash

# Define paths
UPLOAD_DIR="/var/reports/uploads"
REPORT_DIR="/var/reports/reporting"
BACKUP_DIR="/var/reports/backup"
LOG_FILE="/var/log/report_daemon.log"

echo "Initializing Report Daemon Environment..."

# Create directories if they do not exist
for dir in "$UPLOAD_DIR" "$REPORT_DIR" "$BACKUP_DIR"; do
    if [ ! -d "$dir" ]; then
        echo "Creating directory: $dir"
        sudo mkdir -p "$dir"
    else
        echo "Directory already exists: $dir"
    fi
    # Set appropriate permissions
    sudo chmod 777 "$dir"
    echo "Permissions set for $dir"
done

# Create or clear the log file
if [ ! -f "$LOG_FILE" ]; then
    echo "Creating log file: $LOG_FILE"
    sudo touch "$LOG_FILE"
else
    echo "Clearing existing log file: $LOG_FILE"
    sudo truncate -s 0 "$LOG_FILE"
fi
sudo chmod 666 "$LOG_FILE"
echo "Permissions set for log file"

echo "Setup complete."