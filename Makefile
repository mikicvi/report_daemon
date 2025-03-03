CC = gcc
CFLAGS = -Wall
PREFIX = /usr/local
SYSTEMD_DIR = /etc/systemd/system

all: report_daemon

report_daemon: src/daemon.c src/logging.c src/file_monitor.c src/backup.c src/utils.c
	$(CC) $(CFLAGS) -o build/report_daemon src/daemon.c src/logging.c src/file_monitor.c src/backup.c src/utils.c -I src

clean:
	rm -f build/report_daemon

install: report_daemon
    # Install binary
	sudo mkdir -p $(PREFIX)/bin
	sudo cp build/report_daemon $(PREFIX)/bin/
	sudo chown root:root $(PREFIX)/bin/report_daemon
	sudo chmod 755 $(PREFIX)/bin/report_daemon
    
    # Install service file
	sudo cp config/report_daemon.service $(SYSTEMD_DIR)/
	sudo chown root:root $(SYSTEMD_DIR)/report_daemon.service
	sudo chmod 644 $(SYSTEMD_DIR)/report_daemon.service
    
    # Create required directories
	sudo mkdir -p /var/reports/{uploads,reporting,backup}
	sudo chmod 777 /var/reports/{uploads,reporting,backup}
	sudo touch /var/log/report_daemon.log
	sudo chmod 666 /var/log/report_daemon.log
    
    # Reload systemd and enable service
	sudo systemctl daemon-reload
	sudo systemctl enable report_daemon.service

uninstall:
	sudo systemctl stop report_daemon.service
	sudo systemctl disable report_daemon.service
	sudo rm -f $(PREFIX)/bin/report_daemon
	sudo rm -f $(SYSTEMD_DIR)/report_daemon.service


start:
	sudo systemctl start report_daemon