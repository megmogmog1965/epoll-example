FROM alpine:3.10.3

RUN apk update && \
    apk add --no-cache alpine-sdk

COPY main.c /server/

RUN cd /server/ && \
    gcc -o /usr/local/bin/server main.c

EXPOSE 12001

ENTRYPOINT ["/usr/local/bin/server"]
