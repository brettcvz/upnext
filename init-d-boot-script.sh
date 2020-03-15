#! /bin/sh
# /etc/init.d/radio 

### BEGIN INIT INFO
# Provides:          upNext
# Required-Start:    $remote_fs $syslog
# Required-Stop:     $remote_fs $syslog
# Default-Start:     2 3 4 5
# Default-Stop:      0 1 6
# Short-Description: Launches upNext script in a screen session
# Description:       Launches upNext, which shows the upcoming calendar events on an e-ink display
### END INIT INFO

do_start() {
  #We don't check here to see if the screen session is already started, be careful
  screen -d -m -S upNext bash -c "cd /home/pi/upNext; bin/./upNext >> logs/upNext.log"
}

do_stop() {
  screen -X -S upNext quit
}

case "$1" in
  start)
    echo "Starting upNext"
    do_start
    ;;
  stop)
    echo "Stopping upNext"
    do_stop
    ;;
  restart)
    echo "Stopping upNext"
    do_stop
    echo "Starting upNext"
    do_start
    ;;
  *)
    echo "Usage: /etc/init.d/upNext {start|stop|restart}"
    exit 1
    ;;
esac

exit 0
