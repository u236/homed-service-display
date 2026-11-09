FROM    127.0.0.1:5000/homed-service:latest
ARG     TARGETARCH
RUN     mkdir /etc/homed
RUN     ln -s /data/homed-display.conf /etc/homed/homed-display.conf
RUN     ln -s /data/homed.log /var/log/homed.log
COPY    homed-display-$TARGETARCH /usr/bin/homed-display
CMD     ["/usr/bin/homed-display"]
